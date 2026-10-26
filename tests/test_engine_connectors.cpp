#include <gtest/gtest.h>

#include "engine/connectors/ExternalConnector.h"
#include "engine/core/Errors.h"

namespace tividy
{
	namespace
	{
		class FakeConnector : public ExternalConnector
		{
			public:
				explicit FakeConnector(const std::string& n) : name_ref(n) { }
				std::string name() const override { return name_ref; }
				ConnectorResult invoke(const ConnectorRequest&) override
				{
					return ConnectorResult::ok("fake");
				}
			private:
				std::string name_ref;
		};
	}

	TEST(ConnectorResultTest, OkIsSuccessNotRetryable)
	{
		ConnectorResult r = ConnectorResult::ok("done", {{"k", "v"}});
		EXPECT_TRUE(r.success);
		EXPECT_FALSE(r.retryable);
		EXPECT_EQ(r.message, "done");
		ASSERT_EQ(r.payload.size(), 1u);
		EXPECT_EQ(r.payload.at("k"), "v");
	}

	TEST(ConnectorResultTest, FailureIsNotSuccess)
	{
		ConnectorResult r = ConnectorResult::failure("boom", true);
		EXPECT_FALSE(r.success);
		EXPECT_TRUE(r.retryable);
		EXPECT_EQ(r.message, "boom");
		EXPECT_TRUE(r.payload.empty());
	}

	TEST(ConnectorResultTest, FailureCarriesRetryableFlag)
	{
		EXPECT_FALSE(ConnectorResult::failure("permanent", false).retryable);
		EXPECT_TRUE(ConnectorResult::failure("transient", true).retryable);
	}

	TEST(ConnectorRegistryTest, HasReturnsFalseWhenEmpty)
	{
		ConnectorRegistry reg;
		EXPECT_FALSE(reg.has("anything"));
	}

	TEST(ConnectorRegistryTest, RegisterThenHas)
	{
		ConnectorRegistry reg;
		reg.registerConnector(std::unique_ptr<ExternalConnector>(new FakeConnector("a")));
		EXPECT_TRUE(reg.has("a"));
		EXPECT_FALSE(reg.has("b"));
	}

	TEST(ConnectorRegistryTest, RegisterDuplicateThrows)
	{
		ConnectorRegistry reg;
		reg.registerConnector(std::unique_ptr<ExternalConnector>(new FakeConnector("a")));
		EXPECT_THROW(
			reg.registerConnector(std::unique_ptr<ExternalConnector>(new FakeConnector("a"))),
			EngineError
		);
	}

	TEST(ConnectorRegistryTest, RequireReturnsRegisteredConnector)
	{
		ConnectorRegistry reg;
		reg.registerConnector(std::unique_ptr<ExternalConnector>(new FakeConnector("a")));
		EXPECT_EQ(reg.require("a").name(), "a");
	}

	TEST(ConnectorRegistryTest, RequireMissingThrows)
	{
		ConnectorRegistry reg;
		EXPECT_THROW(reg.require("missing"), NotFoundError);
	}

	TEST(LoggingNotifierTest, DefaultName)
	{
		LoggingNotifier n;
		EXPECT_EQ(n.name(), "notifier");
	}

	TEST(LoggingNotifierTest, CustomName)
	{
		LoggingNotifier n("custom-name");
		EXPECT_EQ(n.name(), "custom-name");
	}

	TEST(LoggingNotifierTest, NotifyAppendsToSent)
	{
		LoggingNotifier n;
		EXPECT_TRUE(n.sent().empty());

		ConnectorRequest req;
		req.operation = "notify";
		req.params["recipient"] = "u1";
		req.params["subject"] = "hi";
		req.params["body"] = "hello world";

		ConnectorResult r = n.invoke(req);
		EXPECT_TRUE(r.success);
		ASSERT_EQ(n.sent().size(), 1u);
		EXPECT_EQ(n.sent()[0], "u1 | hi | hello world");
	}

	TEST(LoggingNotifierTest, UnknownOperationFailsNonRetryable)
	{
		LoggingNotifier n;
		ConnectorRequest req;
		req.operation = "delete_everything";

		ConnectorResult r = n.invoke(req);
		EXPECT_FALSE(r.success);
		EXPECT_FALSE(r.retryable);
		EXPECT_TRUE(n.sent().empty());
	}
} // namespace tividy