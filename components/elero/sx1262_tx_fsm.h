#pragma once

#include <cstdint>

namespace esphome {
namespace elero {

enum class Sx1262TxState : uint8_t {
  Idle,
  Prepare,
  WaitTxDone,
  ReturnToRx,
  WaitRxReady,
  Recover,
};

enum class Sx1262TxPhaseResult : uint8_t {
  Pending,
  Succeeded,
  Failed,
};

enum class Sx1262TxTerminalResult : uint8_t {
  None,
  Success,
  Failed,
};

class Sx1262TxFsmOwner {
 public:
  virtual ~Sx1262TxFsmOwner() = default;

  virtual bool tx_prepare_for_fsm() = 0;
  virtual Sx1262TxPhaseResult tx_wait_done_for_fsm() = 0;
  virtual bool tx_return_to_rx_for_fsm() = 0;
  virtual Sx1262TxPhaseResult tx_wait_rx_ready_for_fsm() = 0;
  virtual void tx_on_state_enter_for_fsm(Sx1262TxState state, uint32_t now) = 0;
  virtual void tx_set_terminal_result_for_fsm(Sx1262TxTerminalResult result) = 0;
  virtual void tx_recover_for_fsm() = 0;
};

class Sx1262TxFsm {
 public:
  explicit Sx1262TxFsm(Sx1262TxFsmOwner &owner);

  [[nodiscard]] bool Start(uint32_t now);
  void Poll(uint32_t now);
  void Abort(uint32_t now);

  [[nodiscard]] bool is_idle() const { return state_ == Sx1262TxState::Idle; }
  [[nodiscard]] Sx1262TxState state() const { return state_; }

 private:
  void TransitionTo(Sx1262TxState state, uint32_t now);
  [[nodiscard]] bool IsTransitionAllowed_(Sx1262TxState from,
                                          Sx1262TxState to) const;
  void HandlePrepare(uint32_t now);
  void HandleWaitTxDone(uint32_t now);
  void HandleReturnToRx(uint32_t now);
  void HandleWaitRxReady(uint32_t now);
  void HandleRecover(uint32_t now);

  Sx1262TxFsmOwner &owner_;
  Sx1262TxState state_{Sx1262TxState::Idle};
};

}  // namespace elero
}  // namespace esphome
