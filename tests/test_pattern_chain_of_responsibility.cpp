#include <gtest/gtest.h>

#include "engine/policies/EscalationHandler.h"
#include "engine/core/Errors.h"
#include "engine/core/Types.h" 

namespace tividy
{

    namespace 
    {
        class TestHandler : public EscalationHandler
        {
            public:
                explicit TestHandler(std::shared_ptr<const EscalationHandler> successor = nullptr)
                    : EscalationHandler(successor) {}
            public:
                virtual bool canHandle(const EscalationRequest& /*request*/) const
                {
                    return true;
                }
                virtual EscalationResult resolve(const EscalationRequest& /*request*/) const
                {
                    return {true, "test_role", "TestHandler"};
                }
        };

        class UnhandledRequestHandler : public EscalationHandler
        {
            public:
                explicit UnhandledRequestHandler(std::shared_ptr<const EscalationHandler> successor = nullptr)
                    : EscalationHandler(successor) {}
            public:
                virtual bool canHandle(const EscalationRequest& /*request*/) const
                {
                    return false;
                }
                virtual EscalationResult resolve(const EscalationRequest& /*request*/) const
                {
                    return {false, "", ""};
                }
        };
    }

    TEST(EscalationHandlerTest, HandleResolvesRequest)
    {
        TestHandler handler;
        Id instance_id = "instance_1";
        Id item_id = "item_1";
        EscalationRequest request = {instance_id, item_id, "reason", 1, nullptr};
        EscalationResult result = handler.handle(request);
        EXPECT_TRUE(result.resolved);
        EXPECT_EQ(result.approver_role, "test_role");
        EXPECT_EQ(result.handled_by, "TestHandler");
    }

    TEST(EscalationHandlerTest, HandleDelegatesToSuccessor)
    {
        auto successor = std::make_shared<TestHandler>();
        UnhandledRequestHandler handler(successor);
    
        WorkflowData data;
        EscalationRequest request = {
            "instance_1", "item_1", "reason", 1, &data
        };
    
        EscalationResult result = handler.handle(request);
    
        EXPECT_TRUE(result.resolved);
        EXPECT_EQ(result.approver_role, "test_role");
        EXPECT_EQ(result.handled_by, "TestHandler");
    }

    TEST(EscalationHandlerTest, HandleReturnsUnresolvedWhenAllHandlersReject)
    {
        auto successor = std::make_shared<UnhandledRequestHandler>();
        UnhandledRequestHandler handler(successor);
    
        WorkflowData data;
        EscalationRequest request = {
            "instance_1", "item_1", "reason", 1, &data
        };
    
        EscalationResult result{};
        ASSERT_NO_THROW(result = handler.handle(request));
    
        EXPECT_FALSE(result.resolved);
        EXPECT_EQ(result.approver_role, "");
        EXPECT_EQ(result.handled_by, "");
    }

    TEST(EscalationHandlerTest, HandleReturnsUnresolvedIfNoSuccessor)
    {
        UnhandledRequestHandler handler;
        Id instance_id = "instance_1";
        Id item_id = "item_1";
        EscalationRequest request = {instance_id, item_id, "reason", 1, nullptr};
        EscalationResult result = handler.handle(request);
        EXPECT_FALSE(result.resolved);
        EXPECT_EQ(result.approver_role, "");
        EXPECT_EQ(result.handled_by, "");
    }

    TEST(EscalationHandlerTest, HandleDelegatesThroughMultipleSuccessors)
    {
        auto last = std::make_shared<TestHandler>();
        auto middle = std::make_shared<UnhandledRequestHandler>(last);
        UnhandledRequestHandler handler(middle);
    
        WorkflowData data;
        EscalationRequest request = {
            "instance_1", "item_1", "reason", 1, &data
        };
    
        EscalationResult result = handler.handle(request);
    
        EXPECT_TRUE(result.resolved);
        EXPECT_EQ(result.approver_role, "test_role");
        EXPECT_EQ(result.handled_by, "TestHandler");
    }

    TEST(EscalationHandlerTest, HandleUsesFirstMatchingHandler)
    {
        auto successor =
            std::make_shared<RoleEscalationHandler>("director", 1);
        RoleEscalationHandler handler("manager", 1, successor);
    
        WorkflowData data;
        EscalationRequest request = {
            "instance_1", "item_1", "reason", 1, &data
        };
    
        EscalationResult result = handler.handle(request);
    
        EXPECT_TRUE(result.resolved);
        EXPECT_EQ(result.approver_role, "manager");
        EXPECT_EQ(result.handled_by, "manager");
    }

    TEST(EscalationHandlerTest, RoleHandlerRequiresMatchingLevel)
    {
        RoleEscalationHandler handler("director", 2);
    
        WorkflowData data;
        EscalationRequest request = {
            "instance_1", "item_1", "reason", 1, &data
        };
    
        EscalationResult rejected = handler.handle(request);
    
        EXPECT_FALSE(rejected.resolved);
        EXPECT_EQ(rejected.approver_role, "");
        EXPECT_EQ(rejected.handled_by, "");
    
        request.level = 2;
        EscalationResult accepted = handler.handle(request);
    
        EXPECT_TRUE(accepted.resolved);
        EXPECT_EQ(accepted.approver_role, "director");
        EXPECT_EQ(accepted.handled_by, "director");
    }

}