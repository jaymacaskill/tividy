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

    TEST(EscalationHandlerTest, HandleResolvesRequest)
    {
        TestHandler handler;
        EscalationRequest request = {1, 2, "reason", 1, nullptr};
        EscalationResult result = handler.handle(request);
        EXPECT_TRUE(result.resolved);
        EXPECT_EQ(result.approver_role, "test_role");
        EXPECT_EQ(result.handled_by, "TestHandler");
    }

    

}