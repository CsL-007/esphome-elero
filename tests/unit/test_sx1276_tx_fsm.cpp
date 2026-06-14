#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "elero/sx1276_tx_fsm.h"

namespace esphome {
namespace elero {
namespace {

class FakeTxOwner : public Sx1276TxFsmOwner {
 public:
  bool prepare_result{true};
  std::vector<Sx1276TxPhaseResult> wait_done_results{
      Sx1276TxPhaseResult::Succeeded};
  bool return_to_rx_result{true};
  std::vector<Sx1276TxPhaseResult> wait_rx_ready_results{
      Sx1276TxPhaseResult::Succeeded};

  std::vector<Sx1276TxState> entered_states;
  std::vector<Sx1276TxTerminalResult> terminal_results;
  int recover_calls{0};

  bool tx_prepare_for_fsm() override { return prepare_result; }

  Sx1276TxPhaseResult tx_wait_done_for_fsm() override {
    return PopOrLast(wait_done_results);
  }

  bool tx_return_to_rx_for_fsm() override { return return_to_rx_result; }

  Sx1276TxPhaseResult tx_wait_rx_ready_for_fsm() override {
    return PopOrLast(wait_rx_ready_results);
  }

  void tx_on_state_enter_for_fsm(Sx1276TxState state, uint32_t) override {
    entered_states.push_back(state);
  }

  void tx_set_terminal_result_for_fsm(Sx1276TxTerminalResult result) override {
    terminal_results.push_back(result);
  }

  void tx_recover_for_fsm() override { ++recover_calls; }

 private:
  static Sx1276TxPhaseResult PopOrLast(std::vector<Sx1276TxPhaseResult> &results) {
    if (results.empty()) {
      return Sx1276TxPhaseResult::Pending;
    }
    if (results.size() == 1) {
      return results.front();
    }
    Sx1276TxPhaseResult result = results.front();
    results.erase(results.begin());
    return result;
  }
};

TEST(Sx1276TxFsmTest, SuccessPathUsesPrepareWaitDoneAndReturnToRx) {
  FakeTxOwner owner;
  Sx1276TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(100));
  EXPECT_EQ(fsm.state(), Sx1276TxState::Prepare);

  fsm.Poll(101);
  ASSERT_EQ(fsm.state(), Sx1276TxState::WaitTxDone);

  fsm.Poll(102);
  ASSERT_EQ(fsm.state(), Sx1276TxState::WaitRxReady);

  fsm.Poll(103);

  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1276TxState>({Sx1276TxState::Prepare,
                                        Sx1276TxState::WaitTxDone,
                                        Sx1276TxState::ReturnToRx,
                                        Sx1276TxState::WaitRxReady,
                                        Sx1276TxState::Idle}));
  EXPECT_EQ(owner.terminal_results,
            std::vector<Sx1276TxTerminalResult>({Sx1276TxTerminalResult::None,
                                                 Sx1276TxTerminalResult::Success}));
  EXPECT_EQ(owner.recover_calls, 0);
}

TEST(Sx1276TxFsmTest, PrepareFailureRecoversImmediately) {
  FakeTxOwner owner;
  owner.prepare_result = false;
  Sx1276TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(10));
  fsm.Poll(11);

  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1276TxState>({Sx1276TxState::Prepare,
                                        Sx1276TxState::Recover,
                                        Sx1276TxState::Idle}));
  EXPECT_EQ(owner.terminal_results,
            std::vector<Sx1276TxTerminalResult>({Sx1276TxTerminalResult::None,
                                                 Sx1276TxTerminalResult::Failed}));
  EXPECT_EQ(owner.recover_calls, 1);
}

TEST(Sx1276TxFsmTest, WaitDoneCanRemainPendingBeforeFailing) {
  FakeTxOwner owner;
  owner.wait_done_results = {Sx1276TxPhaseResult::Pending,
                             Sx1276TxPhaseResult::Failed};
  Sx1276TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(20));
  fsm.Poll(21);
  ASSERT_EQ(fsm.state(), Sx1276TxState::WaitTxDone);

  fsm.Poll(22);
  EXPECT_EQ(fsm.state(), Sx1276TxState::WaitTxDone);

  fsm.Poll(23);

  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1276TxState>({Sx1276TxState::Prepare,
                                        Sx1276TxState::WaitTxDone,
                                        Sx1276TxState::Recover,
                                        Sx1276TxState::Idle}));
  EXPECT_EQ(owner.terminal_results,
            std::vector<Sx1276TxTerminalResult>({Sx1276TxTerminalResult::None,
                                                 Sx1276TxTerminalResult::Failed}));
  EXPECT_EQ(owner.recover_calls, 1);
}

TEST(Sx1276TxFsmTest, ReturnToRxFailureRoutesThroughRecovery) {
  FakeTxOwner owner;
  owner.return_to_rx_result = false;
  Sx1276TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(30));
  fsm.Poll(31);
  ASSERT_EQ(fsm.state(), Sx1276TxState::WaitTxDone);

  fsm.Poll(32);

  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1276TxState>({Sx1276TxState::Prepare,
                                        Sx1276TxState::WaitTxDone,
                                        Sx1276TxState::ReturnToRx,
                                        Sx1276TxState::Recover,
                                        Sx1276TxState::Idle}));
  EXPECT_EQ(owner.terminal_results,
            std::vector<Sx1276TxTerminalResult>({Sx1276TxTerminalResult::None,
                                                 Sx1276TxTerminalResult::Failed}));
  EXPECT_EQ(owner.recover_calls, 1);
}

TEST(Sx1276TxFsmTest, WaitRxReadyCanRemainPendingBeforeSucceeding) {
  FakeTxOwner owner;
  owner.wait_rx_ready_results = {Sx1276TxPhaseResult::Pending,
                                 Sx1276TxPhaseResult::Succeeded};
  Sx1276TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(40));
  fsm.Poll(41);
  ASSERT_EQ(fsm.state(), Sx1276TxState::WaitTxDone);

  fsm.Poll(42);
  ASSERT_EQ(fsm.state(), Sx1276TxState::WaitRxReady);

  fsm.Poll(43);
  EXPECT_EQ(fsm.state(), Sx1276TxState::WaitRxReady);

  fsm.Poll(44);
  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1276TxState>({Sx1276TxState::Prepare,
                                        Sx1276TxState::WaitTxDone,
                                        Sx1276TxState::ReturnToRx,
                                        Sx1276TxState::WaitRxReady,
                                        Sx1276TxState::Idle}));
}

TEST(Sx1276TxFsmTest, WaitRxReadyFailureRoutesThroughRecovery) {
  FakeTxOwner owner;
  owner.wait_rx_ready_results = {Sx1276TxPhaseResult::Failed};
  Sx1276TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(45));
  fsm.Poll(46);
  ASSERT_EQ(fsm.state(), Sx1276TxState::WaitTxDone);

  fsm.Poll(47);
  ASSERT_EQ(fsm.state(), Sx1276TxState::WaitRxReady);

  fsm.Poll(48);
  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1276TxState>({Sx1276TxState::Prepare,
                                        Sx1276TxState::WaitTxDone,
                                        Sx1276TxState::ReturnToRx,
                                        Sx1276TxState::WaitRxReady,
                                        Sx1276TxState::Recover,
                                        Sx1276TxState::Idle}));
  EXPECT_EQ(owner.terminal_results,
            std::vector<Sx1276TxTerminalResult>({Sx1276TxTerminalResult::None,
                                                 Sx1276TxTerminalResult::Failed}));
  EXPECT_EQ(owner.recover_calls, 1);
}

TEST(Sx1276TxFsmTest, AbortRecoversFromIdleAndActiveTx) {
  {
    FakeTxOwner owner;
    Sx1276TxFsm fsm(owner);

    fsm.Abort(40);

    EXPECT_TRUE(fsm.is_idle());
    EXPECT_EQ(owner.recover_calls, 1);
    EXPECT_EQ(owner.terminal_results,
              std::vector<Sx1276TxTerminalResult>({Sx1276TxTerminalResult::Failed}));
  }

  {
    FakeTxOwner owner;
    owner.wait_done_results = {Sx1276TxPhaseResult::Pending};
    Sx1276TxFsm fsm(owner);

    ASSERT_TRUE(fsm.Start(50));
    fsm.Poll(51);
    ASSERT_EQ(fsm.state(), Sx1276TxState::WaitTxDone);

    fsm.Abort(52);

    EXPECT_TRUE(fsm.is_idle());
    EXPECT_EQ(owner.entered_states,
              std::vector<Sx1276TxState>({Sx1276TxState::Prepare,
                                          Sx1276TxState::WaitTxDone,
                                          Sx1276TxState::Recover,
                                          Sx1276TxState::Idle}));
    EXPECT_EQ(owner.terminal_results,
              std::vector<Sx1276TxTerminalResult>({Sx1276TxTerminalResult::None,
                                                   Sx1276TxTerminalResult::Failed}));
    EXPECT_EQ(owner.recover_calls, 1);
  }
}

}  // namespace
}  // namespace elero
}  // namespace esphome
