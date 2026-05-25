#include <gtest/gtest.h>

#include <atomic>

#define ESP_LOGV(tag, format, ...) ((void) 0)
#define ESP_LOGVV(tag, format, ...) ((void) 0)
#define ESP_LOGD(tag, format, ...) ((void) 0)
#define ESP_LOGI(tag, format, ...) ((void) 0)
#define ESP_LOGW(tag, format, ...) ((void) 0)
#define ESP_LOGE(tag, format, ...) ((void) 0)
#define ESP_LOGCONFIG(tag, format, ...) ((void) 0)

#ifndef UNIT_TEST
#define UNIT_TEST
#endif

#include "elero/time_provider.h"

namespace esphome {

uint32_t millis() { return esphome::elero::get_time_provider().millis(); }
void delay(uint32_t) {}
void delay_microseconds_safe(uint32_t) {}

}  // namespace esphome

extern "C" void esp_rom_delay_us(uint32_t) {}

#include "elero/time_provider.cpp"
#include "elero/cc1101_tx_fsm.cpp"
#include "elero/cc1101_driver.cpp"

using namespace esphome::elero;

class Cc1101DriverTxHelperTest : public ::testing::Test {
 protected:
  MockTimeProvider time_;
  CC1101Driver driver_;
  std::atomic<bool> rx_ready_{false};
  std::atomic<bool> tx_done_{false};

  void SetUp() override {
    set_time_provider(&time_);
    time_.reset();
    tx_done_.store(false, std::memory_order_release);
    rx_ready_.store(false, std::memory_order_release);
    driver_.set_irq_flags(&rx_ready_, &tx_done_);
    esphome::spi::test_support::reset();
  }

  void TearDown() override {
    esphome::spi::test_support::reset();
    set_time_provider(nullptr);
  }
};

TEST_F(Cc1101DriverTxHelperTest, WaitTxDoneSucceedsViaMarcstateFallbackWhenIrqMissed) {
  esphome::spi::test_support::read_bytes = {
      CC1101_MARCSTATE_RX,
      0x00,
      0x00,
  };

  driver_.tx_on_state_enter_for_fsm(Cc1101TxState::WaitTxDone, time_.millis());

  EXPECT_EQ(driver_.tx_wait_done_for_fsm(), Cc1101TxPhaseResult::Succeeded);
}

TEST_F(Cc1101DriverTxHelperTest, WaitTxDoneFailsWhenIrqFiresButTxbytesRemainNonZero) {
  tx_done_.store(true, std::memory_order_release);
  esphome::spi::test_support::read_bytes = {
      0x01,
      0x01,
      0x01,
      0x01,
  };

  driver_.tx_on_state_enter_for_fsm(Cc1101TxState::WaitTxDone, time_.millis());

  EXPECT_EQ(driver_.tx_wait_done_for_fsm(), Cc1101TxPhaseResult::Failed);
}

TEST_F(Cc1101DriverTxHelperTest, ReturnToRxStaysPendingWhenMarcstateIsTxEnd) {
  esphome::spi::test_support::read_bytes = {
      CC1101_MARCSTATE_TX_END,
  };

  driver_.tx_set_mode_for_fsm(RadioMode::TX);
  driver_.tx_on_state_enter_for_fsm(Cc1101TxState::ReturnToRx, time_.millis());

  EXPECT_EQ(driver_.tx_return_to_rx_for_fsm(), Cc1101TxPhaseResult::Pending);
  EXPECT_EQ(driver_.mode(), RadioMode::TX);
}

TEST_F(Cc1101DriverTxHelperTest, ReturnToRxFailsWhenTransientStateTimesOut) {
  esphome::spi::test_support::read_bytes = {
      CC1101_MARCSTATE_RXTX_SWITCH,
  };

  driver_.tx_set_mode_for_fsm(RadioMode::TX);
  driver_.tx_on_state_enter_for_fsm(Cc1101TxState::ReturnToRx, time_.millis());
  time_.advance(26);

  EXPECT_EQ(driver_.tx_return_to_rx_for_fsm(), Cc1101TxPhaseResult::Failed);
  EXPECT_EQ(driver_.mode(), RadioMode::TX);
}

TEST_F(Cc1101DriverTxHelperTest, ReturnToRxPreservesRxBytesForLaterDrain) {
  esphome::spi::test_support::read_bytes = {
      CC1101_MARCSTATE_RX,
      0x02,
      0x02,
      CC1101_MARCSTATE_RX,
  };

  driver_.tx_set_mode_for_fsm(RadioMode::TX);

  EXPECT_EQ(driver_.tx_return_to_rx_for_fsm(), Cc1101TxPhaseResult::Succeeded);
  EXPECT_TRUE(rx_ready_.load(std::memory_order_acquire));
  EXPECT_EQ(driver_.mode(), RadioMode::RX);
}

TEST_F(Cc1101DriverTxHelperTest, ReturnToRxFailsWhenMarcstateIsClearlyBad) {
  esphome::spi::test_support::read_bytes = {
      CC1101_MARCSTATE_TX,
  };

  EXPECT_EQ(driver_.tx_return_to_rx_for_fsm(), Cc1101TxPhaseResult::Failed);
}
