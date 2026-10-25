#include <gtest/gtest.h>

#include "engine/policies/EscalationHandler.h"
#include "engine/core/Errors.h"

namespace tividy
{

    class TestHandler : public EscalationHandler
    {
        public:
            explicit TestHandler(std::shared_ptr<const EscalationHandler> successor = nullptr)
                : EscalationHandler(successor) {}
        protected:
            virtual bool canHandle(const EscalationRequest& request) const
            {
                return true;
            }
            virtual EscalationResult resolve(const EscalationRequest& request) const
            {
                return {true, "test_role", "TestHandler"};
            }
    };

    class UnhandledRequestHandler : public EscalationHandler
    {
        public:
            explicit UnhandledRequestHandler(std::shared_ptr<const EscalationHandler> successor = nullptr)
                : EscalationHandler(successor) {}
        protected:
            virtual bool canHandle(const EscalationRequest& request) const
            {
                return false;
            }
            virtual EscalationResult resolve(const EscalationRequest& request) const
            {
                return {false, "", ""};
            }
    };

    TEST(EscalationHandlerTest, HandleResolvesRequest)
    {
        TestHandler handler;
        EscalationRequest request = {1, 2, "reason", 1, nullptr};
        EscalationResult result = handler.handle(request);
        EXPECT_TRUE(result.resolved);
        EXPECT_EQ(result.approver_role, "test_role");
        EXPECT_EQ(result.handled_by, "TestHandler");
    }

    TEST(EscalationHandlerTest, HandleDelegatesToSuccessor)
    {
        TestHandler successor;
        TestHandler handler (std::make_shared<TestHandler>(successor));
        EscalationRequest request = {1, 2, "reason", 1, nullptr};
        EscalationResult result = handler.handle(request);
        EXPECT_TRUE(result.resolved);
        EXPECT_EQ(result.approver_role, "test_role");
        EXPECT_EQ(result.handled_by, "TestHandler");
    }

    TEST(EscalationHandlerTest, HandleThrowsIfNoHandlerCanHandle)
    {
        UnhandledRequestHandler handler;
        EscalationRequest request = {1, 2, "reason", 1, nullptr};
        EXPECT_THROW(handler.handle(request), std::runtime_error);
    }

    TEST(EscalationHandlerTest, HandleReturnsUnresolvedIfNoSuccessor)
    {
        UnhandledRequestHandler handler;
        EscalationRequest request = {1, 2, "reason", 1, nullptr};
        EscalationResult result = handler.handle(request);
        EXPECT_FALSE(result.resolved);
        EXPECT_EQ(result.approver_role, "");
        EXPECT_EQ(result.handled_by, "");
    }

}