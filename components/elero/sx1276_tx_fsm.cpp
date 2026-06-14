#include "sx1276_tx_fsm.h"

#include <cassert>

namespace esphome {
namespace elero {

Sx1276TxFsm::Sx1276TxFsm(Sx1276TxFsmOwner &owner) : owner_(owner) {}

bool Sx1276TxFsm::Start(uint32_t now) {
  if (!this->is_idle()) {
    return false;
  }

  this->owner_.tx_set_terminal_result_for_fsm(Sx1276TxTerminalResult::None);
  this->TransitionTo(Sx1276TxState::Prepare, now);
  return true;
}

void Sx1276TxFsm::Poll(uint32_t now) {
  switch (this->state_) {
    case Sx1276TxState::Idle:
      return;
    case Sx1276TxState::Prepare:
      this->HandlePrepare(now);
      return;
    case Sx1276TxState::WaitTxDone:
      this->HandleWaitTxDone(now);
      return;
    case Sx1276TxState::ReturnToRx:
      this->HandleReturnToRx(now);
      return;
    case Sx1276TxState::WaitRxReady:
      this->HandleWaitRxReady(now);
      return;
    case Sx1276TxState::Recover:
      this->HandleRecover(now);
      return;
  }
}

void Sx1276TxFsm::Abort(uint32_t now) {
  if (this->is_idle()) {
    this->owner_.tx_recover_for_fsm();
    this->owner_.tx_set_terminal_result_for_fsm(Sx1276TxTerminalResult::Failed);
    return;
  }

  this->TransitionTo(Sx1276TxState::Recover, now);
  this->HandleRecover(now);
}

void Sx1276TxFsm::TransitionTo(Sx1276TxState state, uint32_t now) {
#ifndef NDEBUG
  assert(this->IsTransitionAllowed_(this->state_, state));
#endif
  this->state_ = state;
  this->owner_.tx_on_state_enter_for_fsm(state, now);
}

bool Sx1276TxFsm::IsTransitionAllowed_(Sx1276TxState from,
                                       Sx1276TxState to) const {
  switch (from) {
    case Sx1276TxState::Idle:
      return to == Sx1276TxState::Prepare;
    case Sx1276TxState::Prepare:
      return to == Sx1276TxState::WaitTxDone || to == Sx1276TxState::Recover;
    case Sx1276TxState::WaitTxDone:
      return to == Sx1276TxState::ReturnToRx || to == Sx1276TxState::Recover;
    case Sx1276TxState::ReturnToRx:
      return to == Sx1276TxState::WaitRxReady || to == Sx1276TxState::Recover;
    case Sx1276TxState::WaitRxReady:
      return to == Sx1276TxState::Idle || to == Sx1276TxState::Recover;
    case Sx1276TxState::Recover:
      return to == Sx1276TxState::Idle;
  }
  return false;
}

void Sx1276TxFsm::HandlePrepare(uint32_t now) {
  if (!this->owner_.tx_prepare_for_fsm()) {
    this->TransitionTo(Sx1276TxState::Recover, now);
    this->HandleRecover(now);
    return;
  }

  this->TransitionTo(Sx1276TxState::WaitTxDone, now);
}

void Sx1276TxFsm::HandleWaitTxDone(uint32_t now) {
  switch (this->owner_.tx_wait_done_for_fsm()) {
    case Sx1276TxPhaseResult::Pending:
      return;
    case Sx1276TxPhaseResult::Succeeded:
      this->TransitionTo(Sx1276TxState::ReturnToRx, now);
      this->HandleReturnToRx(now);
      return;
    case Sx1276TxPhaseResult::Failed:
      this->TransitionTo(Sx1276TxState::Recover, now);
      this->HandleRecover(now);
      return;
  }
}

void Sx1276TxFsm::HandleReturnToRx(uint32_t now) {
  if (this->owner_.tx_return_to_rx_for_fsm()) {
    this->TransitionTo(Sx1276TxState::WaitRxReady, now);
    return;
  }

  this->TransitionTo(Sx1276TxState::Recover, now);
  this->HandleRecover(now);
}

void Sx1276TxFsm::HandleWaitRxReady(uint32_t now) {
  switch (this->owner_.tx_wait_rx_ready_for_fsm()) {
    case Sx1276TxPhaseResult::Pending:
      return;
    case Sx1276TxPhaseResult::Succeeded:
      this->owner_.tx_set_terminal_result_for_fsm(Sx1276TxTerminalResult::Success);
      this->TransitionTo(Sx1276TxState::Idle, now);
      return;
    case Sx1276TxPhaseResult::Failed:
      this->TransitionTo(Sx1276TxState::Recover, now);
      this->HandleRecover(now);
      return;
  }
}

void Sx1276TxFsm::HandleRecover(uint32_t now) {
  this->owner_.tx_recover_for_fsm();
  this->owner_.tx_set_terminal_result_for_fsm(Sx1276TxTerminalResult::Failed);
  this->TransitionTo(Sx1276TxState::Idle, now);
}

}  // namespace elero
}  // namespace esphome
