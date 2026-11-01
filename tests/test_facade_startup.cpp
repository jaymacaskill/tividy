#include <gtest/gtest.h>
#include <memory>
#include "engine/core/Errors.h"
#include "engine/facade/Tividy.h"
#include "engine/items/StageItem.h"
#include "engine/items/WorkItemIterator.h"
#include "engine/items/WorkItemVisitor.h"
#ifndef TIVIDY_FACADE_REAL_COLLABORATORS
#include "support/FacadeStartupFixtures.h"
#endif

namespace tividy { namespace {
// A real, cloneable test work item; intentionally supplies no task-state engine.
class StartupLeaf : public WorkItem {
public:
    StartupLeaf() : spec_ref("task", "Test task") {}
    const WorkItemSpec& spec() const override { return spec_ref; }
    ItemStatus status() const override { return ItemStatus::Created; }
    const Id& assignee() const override { return empty; }
    WorkItem* find(const Id& id) override { return id == spec_ref.id() ? this : nullptr; }
    void publish() override {}
    void assign(const Id&) override {}
    void start(const Participant&) override {}
    void submit(const Participant&, const Outcome&) override {}
    void complete(const Participant&, const Outcome&) override {}
    void approve(const Participant&, const Outcome&) override {}
    void reject(const Participant&, const std::string&) override {}
    void escalate(const std::string&) override {}
    void cancel(const std::string&) override {}
    void skip(const std::string&) override {}
    void expire(const std::string&, const std::string&) override {}
    void bind(WorkContext&) override {}
    void accept(WorkItemVisitor&) const override {}
    std::unique_ptr<WorkItem> clone() const override {
        return std::unique_ptr<WorkItem>(new StartupLeaf);
    }
    std::unique_ptr<WorkItemIterator> createIterator() override {
        return std::unique_ptr<WorkItemIterator>(new DepthFirstIterator(*this));
    }
private:
    WorkItemSpec spec_ref;
    Id empty;
};
class StartupDirector : public WorkflowDirector {
public:
    explicit StartupDirector(bool fail = false, const std::string& workflow_name = "startup-test")
        : fail(fail), workflow_name(workflow_name) {}
    std::string workflowName() const override { return workflow_name; }
    unsigned version() const override { return 3; }
    std::string description() const override { return "Facade startup contract"; }
    void construct(WorkflowBuilder& builder) const override {
        if (fail) throw InvalidDefinitionError("deliberate build failure");
        builder.addTask(std::unique_ptr<WorkItem>(new StartupLeaf));
    }
private:
    bool fail;
    std::string workflow_name;
};
struct PackProbe { unsigned directors = 0; LoggingNotifier* notifier = nullptr; };
class StartupPack : public IndustryPack {
public:
    StartupPack(std::shared_ptr<PackProbe> probe, bool fail = false) : probe(probe), fail(fail) {}
    std::string key() const override { return "startup"; }
    std::string displayName() const override { return "Startup pack"; }
    std::unique_ptr<WorkflowDirector> createDirector() const override {
        ++probe->directors;
        return std::unique_ptr<WorkflowDirector>(new StartupDirector(fail));
    }
    std::vector<Participant> createParticipants() const override {
        return {Participant("test-user", "Test user", {"tester"})};
    }
    std::vector<std::unique_ptr<ExternalConnector> > createConnectors() const override {
        std::vector<std::unique_ptr<ExternalConnector> > result;
        probe->notifier = new LoggingNotifier;
        result.emplace_back(probe->notifier);
        return result;
    }
    ClientProfile createProfile() const override {
        ClientProfile profile;
        profile.product_name = "Startup brand";
        return profile;
    }
private:
    std::shared_ptr<PackProbe> probe;
    bool fail;
};
// Representative providers: the real Demo/Banking/Telecom implementations are
// not linked by this isolated facade test. Use the real default factory and
// real registry/notifier/observer, with distinct industry connector fixtures.
class IndustryConnector : public ExternalConnector {
public:
    explicit IndustryConnector(const std::string& key) : key(key) {}
    std::string name() const override { return key; }
    ConnectorResult invoke(const ConnectorRequest&) override { return ConnectorResult::ok(key); }
private:
    std::string key;
};
class CustomNotifier : public LoggingNotifier {
public:
    ConnectorResult invoke(const ConnectorRequest&) override {
        return ConnectorResult::ok("custom notification delivery");
    }
};
class ConnectorPack : public IndustryPack {
public:
    ConnectorPack(const std::string& key, const std::vector<std::string>& connectors = {},
                  bool custom_notifier = false)
        : key_ref(key), connector_names(connectors), custom_notifier(custom_notifier) {}
    std::string key() const override { return key_ref; }
    std::string displayName() const override { return key_ref; }
    std::unique_ptr<WorkflowDirector> createDirector() const override {
        return std::unique_ptr<WorkflowDirector>(new StartupDirector(false, key_ref + "-workflow"));
    }
    std::vector<std::unique_ptr<ExternalConnector> > createConnectors() const override {
        std::vector<std::unique_ptr<ExternalConnector> > result;
        if (custom_notifier)
            result.emplace_back(new CustomNotifier);
        else
            result = IndustryPack::createConnectors();
        for (const auto& name : connector_names)
            result.emplace_back(new IndustryConnector(name));
        return result;
    }
private:
    std::string key_ref;
    std::vector<std::string> connector_names;
    bool custom_notifier;
};
class FacadeStartupTest : public ::testing::Test {
protected:
    Tividy engine;
    std::shared_ptr<PackProbe> probe = std::make_shared<PackProbe>();
    void install() { engine.installPack(std::unique_ptr<IndustryPack>(new StartupPack(probe))); }
};
TEST_F(FacadeStartupTest, InstallsDefinitionProfileAndSharedServices) {
    install();
    EXPECT_EQ(engine.packKeys(), std::vector<std::string>({"startup"}));
    EXPECT_EQ(engine.workflowNames(), std::vector<std::string>({"startup-test"}));
    EXPECT_EQ(engine.packOf("startup-test"), "startup");
    EXPECT_EQ(engine.profile("startup").product_name, "Startup brand");
    EXPECT_EQ(probe->directors, 1u);
    // Reject a duplicate key before invoking any of its product factories.
    EXPECT_THROW(engine.installPack(std::unique_ptr<IndustryPack>(new StartupPack(probe))), EngineError);
    EXPECT_EQ(probe->directors, 1u); // duplicate rejected before building again
}
TEST_F(FacadeStartupTest, RejectsNullPackWithoutChangingRegistries) {
    EXPECT_THROW(engine.installPack(std::unique_ptr<IndustryPack>()), EngineError);
    EXPECT_TRUE(engine.packKeys().empty());
    EXPECT_TRUE(engine.workflowNames().empty());
}
TEST_F(FacadeStartupTest, FailedBuildDoesNotReservePackKeyOrRegisterCollaborators) {
    EXPECT_THROW(engine.installPack(std::unique_ptr<IndustryPack>(new StartupPack(probe, true))), InvalidDefinitionError);
    EXPECT_TRUE(engine.packKeys().empty());
    EXPECT_TRUE(engine.workflowNames().empty());
    EXPECT_NO_THROW(install()); // same pack/participant/connector ids remain usable
}
TEST_F(FacadeStartupTest, StartsRegisteredDefinitionAndAttachesNotificationObserver) {
    install();
    WorkflowData input;
    input.set("owner", "first");
    const Id id = engine.startWorkflow("startup-test", input);
    WorkflowInstance& instance = engine.instance(id);
    EXPECT_EQ(id, "inst-1");
    EXPECT_EQ(instance.id(), id);
    EXPECT_EQ(instance.definitionName(), "startup-test");
    EXPECT_EQ(instance.definitionVersion(), 3u);
    EXPECT_EQ(instance.status(), InstanceStatus::Running);
    EXPECT_EQ(instance.data().get("owner"), "first");
    EXPECT_NE(instance.context().directory().find("test-user"), nullptr);
    EXPECT_TRUE(instance.context().connectors().has("notifier"));
    instance.events().notify(WorkflowEvent(EventType::ItemAssigned, id, "task", TimePoint()).byActor("test-user"));
    ASSERT_NE(probe->notifier, nullptr);
    ASSERT_EQ(probe->notifier->sent().size(), 1u);
    EXPECT_NE(probe->notifier->sent()[0].find("ItemAssigned"), std::string::npos);
}
TEST_F(FacadeStartupTest, UnknownWorkflowDoesNotConsumeAnIdOrRecordAnInstance) {
    // Adapts #75's proposed unknown-workflow check, adding the counter boundary.
    EXPECT_THROW(engine.startWorkflow("missing", WorkflowData()), NotFoundError);
    EXPECT_TRUE(engine.instanceIds().empty());
    install();
    EXPECT_EQ(engine.startWorkflow("startup-test", WorkflowData()), "inst-1");
}
TEST_F(FacadeStartupTest, StoresSeparateInstancesAndPreservesCreationOrder) {
    install();
    WorkflowData input;
    input.set("owner", "original");
    const Id first = engine.startWorkflow("startup-test", input);
    const Id second = engine.startWorkflow("startup-test", input);
    EXPECT_EQ(engine.instanceIds(), std::vector<Id>({first, second}));
    EXPECT_NE(&engine.instance(first), &engine.instance(second));
    EXPECT_NE(&engine.instance(first).root(), &engine.instance(second).root());
    engine.instance(first).data().set("owner", "changed");
    EXPECT_EQ(engine.instance(second).data().get("owner"), "original");
    EXPECT_EQ(input.get("owner"), "original");
    const Tividy& readOnly = engine;
    EXPECT_EQ(&readOnly.instance(second), &engine.instance(second));
}
TEST_F(FacadeStartupTest, DemoBankingTelecomFixturesShareOneWorkingDefaultNotifier) {
    engine.installPack(std::unique_ptr<IndustryPack>(new ConnectorPack("demo")));
    const Id demo = engine.startWorkflow("demo-workflow", WorkflowData());
    ConnectorRegistry& registry = engine.instance(demo).context().connectors();
    LoggingNotifier* shared = dynamic_cast<LoggingNotifier*>(&registry.require("notifier"));
    ASSERT_NE(shared, nullptr);
    engine.instance(demo).events().notify(WorkflowEvent(EventType::ItemAssigned, demo, "task", TimePoint())
        .byActor(demo).withMessage("shared delivery"));

    ASSERT_NO_THROW(engine.installPack(std::unique_ptr<IndustryPack>(new ConnectorPack("banking"))));
    ASSERT_NO_THROW(engine.installPack(std::unique_ptr<IndustryPack>(new ConnectorPack("telecom"))));
    EXPECT_EQ(&registry.require("notifier"), shared);
    EXPECT_EQ(engine.packKeys(), std::vector<std::string>({"banking", "demo", "telecom"}));
    const Id banking = engine.startWorkflow("banking-workflow", WorkflowData());
    const Id telecom = engine.startWorkflow("telecom-workflow", WorkflowData());
    for (const Id& id : {banking, telecom}) {
        WorkflowInstance& instance = engine.instance(id);
        EXPECT_EQ(&instance.context().connectors().require("notifier"), shared);
        instance.events().notify(WorkflowEvent(EventType::ItemAssigned, id, "task", TimePoint())
            .byActor(id).withMessage("shared delivery"));
    }
    ASSERT_EQ(shared->sent().size(), 3u);
    for (const auto& line : shared->sent())
        EXPECT_NE(line.find("ItemAssigned | shared delivery"), std::string::npos);
}
TEST_F(FacadeStartupTest, SharedNotifierDoesNotPreventDistinctIndustryConnectorRegistration) {
    engine.installPack(std::unique_ptr<IndustryPack>(
        new ConnectorPack("banking", {"core-banking", "card-scheme"})));
    ASSERT_NO_THROW(engine.installPack(std::unique_ptr<IndustryPack>(
        new ConnectorPack("telecom", {"credit-bureau", "network-provisioning"}))));
    const Id id = engine.startWorkflow("banking-workflow", WorkflowData());
    ConnectorRegistry& registry = engine.instance(id).context().connectors();
    for (const std::string& key : {"core-banking", "card-scheme", "credit-bureau", "network-provisioning"}) {
        ASSERT_TRUE(registry.has(key));
        EXPECT_EQ(registry.require(key).invoke(ConnectorRequest()).message, key);
    }
}
TEST_F(FacadeStartupTest, RejectsGenuinelyConflictingIndustryConnectorNames) {
    engine.installPack(std::unique_ptr<IndustryPack>(new ConnectorPack("first", {"industry-api"})));
    const Id id = engine.startWorkflow("first-workflow", WorkflowData());
    ConnectorRegistry& registry = engine.instance(id).context().connectors();
    ExternalConnector* original = &registry.require("industry-api");
    try {
        engine.installPack(std::unique_ptr<IndustryPack>(new ConnectorPack("second", {"industry-api"})));
        FAIL() << "Expected a connector conflict";
    } catch (const EngineError& error) {
        EXPECT_EQ(std::string(error.what()), "Connector industry-api already present in this registry");
    }
    EXPECT_EQ(&registry.require("industry-api"), original);
    EXPECT_EQ(engine.packKeys(), std::vector<std::string>({"first"}));
    EXPECT_EQ(engine.workflowNames(), std::vector<std::string>({"first-workflow"}));
}
TEST_F(FacadeStartupTest, RejectsCustomNotifierCollisionWithDefaultNotifier) {
    engine.installPack(std::unique_ptr<IndustryPack>(new ConnectorPack("default")));
    const Id id = engine.startWorkflow("default-workflow", WorkflowData());
    ConnectorRegistry& registry = engine.instance(id).context().connectors();
    ExternalConnector* original = &registry.require("notifier");
    EXPECT_THROW(engine.installPack(std::unique_ptr<IndustryPack>(
        new ConnectorPack("custom", {}, true))), EngineError);
    EXPECT_EQ(&registry.require("notifier"), original);
    EXPECT_EQ(engine.packKeys(), std::vector<std::string>({"default"}));
}
TEST_F(FacadeStartupTest, PreservesSingleCustomNotifierAndRejectsConflictingDefault) {
    engine.installPack(std::unique_ptr<IndustryPack>(new ConnectorPack("custom", {}, true)));
    const Id id = engine.startWorkflow("custom-workflow", WorkflowData());
    ConnectorRegistry& registry = engine.instance(id).context().connectors();
    ExternalConnector* original = &registry.require("notifier");
    EXPECT_EQ(original->invoke(ConnectorRequest()).message, "custom notification delivery");
    EXPECT_THROW(engine.installPack(std::unique_ptr<IndustryPack>(new ConnectorPack("default"))), EngineError);
    EXPECT_EQ(&registry.require("notifier"), original);
    EXPECT_EQ(engine.packKeys(), std::vector<std::string>({"custom"}));
}
TEST_F(FacadeStartupTest, RegistryStillRejectsDirectDefaultNotifierDuplicates) {
    engine.installPack(std::unique_ptr<IndustryPack>(new ConnectorPack("default")));
    const Id id = engine.startWorkflow("default-workflow", WorkflowData());
    ConnectorRegistry& registry = engine.instance(id).context().connectors();
    ExternalConnector* original = &registry.require("notifier");
    EXPECT_THROW(registry.registerConnector(std::unique_ptr<ExternalConnector>(new LoggingNotifier)), EngineError);
    EXPECT_EQ(&registry.require("notifier"), original);
}
} } // namespace tividy
