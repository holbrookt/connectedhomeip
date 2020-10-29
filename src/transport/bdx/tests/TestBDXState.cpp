#include <transport/bdx/BDXMessages.h>
#include <transport/bdx/BDXState.h>

#include <nlunit-test.h>

#include <core/CHIPCore.h>
#include <support/TestUtils.h>

using namespace chip;

static uint16_t lastMessageSent = BDX::MessageTypes::kStatusReport;

CHIP_ERROR MockSendMessageCallback(uint16_t msgType, System::PacketBuffer * msgData)
{
    printf("BDXState machine sent message %x\n", msgType);
    lastMessageSent = msgType;
    return CHIP_NO_ERROR;
}

void TestHandleReceiveInitSenderOnly(nlTestSuite * inSuite, void * inContext)
{
    // Initialize state machine
    BDX::TransferParams params = { BDX::TransferRole::kReceiver, true, true };
    BDX::BDXState bdxReceiver(params, MockSendMessageCallback);

    // send mock message
    System::PacketBuffer * p = System::PacketBuffer::New(10);
    bdxReceiver.HandleMessageReceived(BDX::MessageTypes::kReceiveInit, p);
    NL_TEST_ASSERT(inSuite, lastMessageSent == BDX::MessageTypes::kStatusReport);

    params = { BDX::TransferRole::kSender, true, true };
    BDX::BDXState bdxSender(params, MockSendMessageCallback);

    bdxSender.HandleMessageReceived(BDX::MessageTypes::kReceiveInit, p);
    NL_TEST_ASSERT(inSuite, lastMessageSent == BDX::MessageTypes::kBlock);
}

// Test Suite

/**
 *  Test Suite that lists all the test functions.
 */
// clang-format off
static const nlTest sTests[] =
{
    NL_TEST_DEF("ReceiveInitSenderOnly", TestHandleReceiveInitSenderOnly),

    NL_TEST_SENTINEL()
};
// clang-format on

// clang-format off
static nlTestSuite sSuite =
{
    "Test-CHIP-BDXState",
    &sTests[0],
    nullptr,
    nullptr
};
// clang-format on

/**
 *  Main
 */
int TestBDXState()
{
    // Run test suit against one context
    nlTestRunner(&sSuite, nullptr);

    return (nlTestRunnerStats(&sSuite));
}

CHIP_REGISTER_TEST_SUITE(TestBDXState)
