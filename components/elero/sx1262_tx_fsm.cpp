#include "sx1262_tx_fsm.h"

#include <cassert>

namespace esphome {
namespace elero {

Sx1262TxFsm::Sx1262TxFsm(Sx1262TxFsmOwner &owner) : owner_(owner) {}

bool Sx1262TxFsm::Start(uint32_t now) {
  if (!this->is_idle()) {
    return false;
  }

  this->owner_.tx_set_terminal_result_for_fsm(Sx1262TxTerminalResult::None);
  this->TransitionTo(Sx1262TxState::Prepare, now);
  return true;
}

void Sx1262TxFsm::Poll(uint32_t now) {
  switch (this->state_) {
    case Sx1262TxState::Idle:
      return;
    case Sx1262TxState::Prepare:
      this->HandlePrepare(now);
      return;
    case Sx1262TxState::WaitTxDone:
      this->HandleWaitTxDone(now);
      return;
    case Sx1262TxState::ReturnToRx:
      this->HandleReturnToRx(now);
      return;
    case Sx1262TxState::WaitRxReady:
      this->HandleWaitRxReady(now);
      return;
    case Sx1262TxState::Recover:
      this->HandleRecover(now);
      return;
  }
}

void Sx1262TxFsm::Abort(uint32_t now) {
  if (this->is_idle()) {
    this->owner_.tx_recover_for_fsm();
    this->owner_.tx_set_terminal_result_for_fsm(Sx1262TxTerminalResult::Failed);
    return;
  }

  this->TransitionTo(Sx1262TxState::Recover, now);
  this->HandleRecover(now);
}

void Sx1262TxFsm::TransitionTo(Sx1262TxState state, uint32_t now) {
#ifndef NDEBUG
  assert(this->IsTransitionAllowed_(this->state_, state));
#endif
  this->state_ = state;
  this->owner_.tx_on_state_enter_for_fsm(state, now);
}

bool Sx1262TxFsm::IsTransitionAllowed_(Sx1262TxState from,
                                       Sx1262TxState to) const {
  switch (from) {
    case Sx1262TxState::Idle:
      return to == Sx1262TxState::Prepare;
    case Sx1262TxState::Prepare:
      return to == Sx1262TxState::WaitTxDone || to == Sx1262TxState::Recover;
    case Sx1262TxState::WaitTxDone:
      return to == Sx1262TxState::ReturnToRx || to == Sx1262TxState::Recover;
    case Sx1262TxState::ReturnToRx:
      return to == Sx1262TxState::WaitRxReady || to == Sx1262TxState::Recover;
    case Sx1262TxState::WaitRxReady:
      return to == Sx1262TxState::Idle || to == Sx1262TxState::Recover;
    case Sx1262TxState::Recover:
      return to == Sx1262TxState::Idle;
  }
  return false;
}

void Sx1262TxFsm::HandlePrepare(uint32_t now) {
  if (!this->owner_.tx_prepare_for_fsm()) {
    this->TransitionTo(Sx1262TxState::Recover, now);
    this->HandleRecover(now);
    return;
  }

  this->TransitionTo(Sx1262TxState::WaitTxDone, now);
}

void Sx1262TxFsm::HandleWaitTxDone(uint32_t now) {
  switch (this->owner_.tx_wait_done_for_fsm()) {
    case Sx1262TxPhaseResult::Pending:
      return;
    case Sx1262TxPhaseResult::Succeeded:
      this->TransitionTo(Sx1262TxState::ReturnToRx, now);
      this->HandleReturnToRx(now);
      return;
    case Sx1262TxPhaseResult::Failed:
      this->TransitionTo(Sx1262TxState::Recover, now);
      this->HandleRecover(now);
      return;
  }
}

void Sx1262TxFsm::HandleReturnToRx(uint32_t now) {
  if (this->owner_.tx_return_to_rx_for_fsm()) {
    this->TransitionTo(Sx1262TxState::WaitRxReady, now);
    return;
  }

  this->TransitionTo(Sx1262TxState::Recover, now);
  this->HandleRecover(now);
}

void Sx1262TxFsm::HandleWaitRxReady(uint32_t now) {
  switch (this->owner_.tx_wait_rx_ready_for_fsm()) {
    case Sx1262TxPhaseResult::Pending:
      return;
    case Sx1262TxPhaseResult::Succeeded:
      this->owner_.tx_set_terminal_result_for_fsm(Sx1262TxTerminalResult::Success);
      this->TransitionTo(Sx1262TxState::Idle, now);
      return;
    case Sx1262TxPhaseResult::Failed:
      this->TransitionTo(Sx1262TxState::Recover, now);
      this->HandleRecover(now);
      return;
  }
}

void Sx1262TxFsm::HandleRecover(uint32_t now) {
  this->owner_.tx_recover_for_fsm();
  this->owner_.tx_set_terminal_result_for_fsm(Sx1262TxTerminalResult::Failed);
  this->TransitionTo(Sx1262TxState::Idle, now);
}

}  // namespace elero
}  // namespace esphome
