#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "elero/sx1262_tx_fsm.h"

namespace esphome {
namespace elero {
namespace {

class FakeTxOwner : public Sx1262TxFsmOwner {
 public:
  bool prepare_result{true};
  std::vector<Sx1262TxPhaseResult> wait_done_results{
      Sx1262TxPhaseResult::Succeeded};
  bool return_to_rx_result{true};
  std::vector<Sx1262TxPhaseResult> wait_rx_ready_results{
      Sx1262TxPhaseResult::Succeeded};

  std::vector<Sx1262TxState> entered_states;
  std::vector<Sx1262TxTerminalResult> terminal_results;
  int recover_calls{0};

  bool tx_prepare_for_fsm() override { return prepare_result; }

  Sx1262TxPhaseResult tx_wait_done_for_fsm() override {
    return PopOrLast(wait_done_results);
  }

  bool tx_return_to_rx_for_fsm() override { return return_to_rx_result; }

  Sx1262TxPhaseResult tx_wait_rx_ready_for_fsm() override {
    return PopOrLast(wait_rx_ready_results);
  }

  void tx_on_state_enter_for_fsm(Sx1262TxState state, uint32_t) override {
    entered_states.push_back(state);
  }

  void tx_set_terminal_result_for_fsm(Sx1262TxTerminalResult result) override {
    terminal_results.push_back(result);
  }

  void tx_recover_for_fsm() override { ++recover_calls; }

 private:
  static Sx1262TxPhaseResult PopOrLast(std::vector<Sx1262TxPhaseResult> &results) {
    if (results.empty()) {
      return Sx1262TxPhaseResult::Pending;
    }
    if (results.size() == 1) {
      return results.front();
    }
    Sx1262TxPhaseResult result = results.front();
    results.erase(results.begin());
    return result;
  }
};

TEST(Sx1262TxFsmTest, SuccessPathUsesPrepareWaitDoneAndReturnToRx) {
  FakeTxOwner owner;
  Sx1262TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(100));
  EXPECT_EQ(fsm.state(), Sx1262TxState::Prepare);

  fsm.Poll(101);
  ASSERT_EQ(fsm.state(), Sx1262TxState::WaitTxDone);

  fsm.Poll(102);
  ASSERT_EQ(fsm.state(), Sx1262TxState::WaitRxReady);

  fsm.Poll(103);

  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1262TxState>({Sx1262TxState::Prepare,
                                        Sx1262TxState::WaitTxDone,
                                        Sx1262TxState::ReturnToRx,
                                        Sx1262TxState::WaitRxReady,
                                        Sx1262TxState::Idle}));
  EXPECT_EQ(owner.terminal_results,
            std::vector<Sx1262TxTerminalResult>({Sx1262TxTerminalResult::None,
                                                 Sx1262TxTerminalResult::Success}));
  EXPECT_EQ(owner.recover_calls, 0);
}

TEST(Sx1262TxFsmTest, PrepareFailureRecoversImmediately) {
  FakeTxOwner owner;
  owner.prepare_result = false;
  Sx1262TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(10));
  fsm.Poll(11);

  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1262TxState>({Sx1262TxState::Prepare,
                                        Sx1262TxState::Recover,
                                        Sx1262TxState::Idle}));
  EXPECT_EQ(owner.terminal_results,
            std::vector<Sx1262TxTerminalResult>({Sx1262TxTerminalResult::None,
                                                 Sx1262TxTerminalResult::Failed}));
  EXPECT_EQ(owner.recover_calls, 1);
}

TEST(Sx1262TxFsmTest, WaitDoneCanRemainPendingBeforeFailing) {
  FakeTxOwner owner;
  owner.wait_done_results = {Sx1262TxPhaseResult::Pending,
                             Sx1262TxPhaseResult::Failed};
  Sx1262TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(20));
  fsm.Poll(21);
  ASSERT_EQ(fsm.state(), Sx1262TxState::WaitTxDone);

  fsm.Poll(22);
  EXPECT_EQ(fsm.state(), Sx1262TxState::WaitTxDone);

  fsm.Poll(23);

  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1262TxState>({Sx1262TxState::Prepare,
                                        Sx1262TxState::WaitTxDone,
                                        Sx1262TxState::Recover,
                                        Sx1262TxState::Idle}));
  EXPECT_EQ(owner.terminal_results,
            std::vector<Sx1262TxTerminalResult>({Sx1262TxTerminalResult::None,
                                                 Sx1262TxTerminalResult::Failed}));
  EXPECT_EQ(owner.recover_calls, 1);
}

TEST(Sx1262TxFsmTest, ReturnToRxFailureRoutesThroughRecovery) {
  FakeTxOwner owner;
  owner.return_to_rx_result = false;
  Sx1262TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(30));
  fsm.Poll(31);
  ASSERT_EQ(fsm.state(), Sx1262TxState::WaitTxDone);

  fsm.Poll(32);

  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1262TxState>({Sx1262TxState::Prepare,
                                        Sx1262TxState::WaitTxDone,
                                        Sx1262TxState::ReturnToRx,
                                        Sx1262TxState::Recover,
                                        Sx1262TxState::Idle}));
  EXPECT_EQ(owner.terminal_results,
            std::vector<Sx1262TxTerminalResult>({Sx1262TxTerminalResult::None,
                                                 Sx1262TxTerminalResult::Failed}));
  EXPECT_EQ(owner.recover_calls, 1);
}

TEST(Sx1262TxFsmTest, WaitRxReadyCanRemainPendingBeforeSucceeding) {
  FakeTxOwner owner;
  owner.wait_rx_ready_results = {Sx1262TxPhaseResult::Pending,
                                 Sx1262TxPhaseResult::Succeeded};
  Sx1262TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(40));
  fsm.Poll(41);
  ASSERT_EQ(fsm.state(), Sx1262TxState::WaitTxDone);

  fsm.Poll(42);
  ASSERT_EQ(fsm.state(), Sx1262TxState::WaitRxReady);

  fsm.Poll(43);
  EXPECT_EQ(fsm.state(), Sx1262TxState::WaitRxReady);

  fsm.Poll(44);
  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1262TxState>({Sx1262TxState::Prepare,
                                        Sx1262TxState::WaitTxDone,
                                        Sx1262TxState::ReturnToRx,
                                        Sx1262TxState::WaitRxReady,
                                        Sx1262TxState::Idle}));
}

TEST(Sx1262TxFsmTest, WaitRxReadyFailureRoutesThroughRecovery) {
  FakeTxOwner owner;
  owner.wait_rx_ready_results = {Sx1262TxPhaseResult::Failed};
  Sx1262TxFsm fsm(owner);

  ASSERT_TRUE(fsm.Start(45));
  fsm.Poll(46);
  ASSERT_EQ(fsm.state(), Sx1262TxState::WaitTxDone);

  fsm.Poll(47);
  ASSERT_EQ(fsm.state(), Sx1262TxState::WaitRxReady);

  fsm.Poll(48);
  EXPECT_TRUE(fsm.is_idle());
  EXPECT_EQ(owner.entered_states,
            std::vector<Sx1262TxState>({Sx1262TxState::Prepare,
                                        Sx1262TxState::WaitTxDone,
                                        Sx1262TxState::ReturnToRx,
                                        Sx1262TxState::WaitRxReady,
                                        Sx1262TxState::Recover,
                                        Sx1262TxState::Idle}));
  EXPECT_EQ(owner.terminal_results,
            std::vector<Sx1262TxTerminalResult>({Sx1262TxTerminalResult::None,
                                                 Sx1262TxTerminalResult::Failed}));
  EXPECT_EQ(owner.recover_calls, 1);
}

TEST(Sx1262TxFsmTest, AbortRecoversFromIdleAndActiveTx) {
  {
    FakeTxOwner owner;
    Sx1262TxFsm fsm(owner);

    fsm.Abort(40);

    EXPECT_TRUE(fsm.is_idle());
    EXPECT_EQ(owner.recover_calls, 1);
    EXPECT_EQ(owner.terminal_results,
              std::vector<Sx1262TxTerminalResult>({Sx1262TxTerminalResult::Failed}));
  }

  {
    FakeTxOwner owner;
    owner.wait_done_results = {Sx1262TxPhaseResult::Pending};
    Sx1262TxFsm fsm(owner);

    ASSERT_TRUE(fsm.Start(50));
    fsm.Poll(51);
    ASSERT_EQ(fsm.state(), Sx1262TxState::WaitTxDone);

    fsm.Abort(52);

    EXPECT_TRUE(fsm.is_idle());
    EXPECT_EQ(owner.entered_states,
              std::vector<Sx1262TxState>({Sx1262TxState::Prepare,
                                          Sx1262TxState::WaitTxDone,
                                          Sx1262TxState::Recover,
                                          Sx1262TxState::Idle}));
    EXPECT_EQ(owner.terminal_results,
              std::vector<Sx1262TxTerminalResult>({Sx1262TxTerminalResult::None,
                                                   Sx1262TxTerminalResult::Failed}));
    EXPECT_EQ(owner.recover_calls, 1);
  }
}

}  // namespace
}  // namespace elero
}  // namespace esphome
