#include <stm32f411/gpio.h>
#include <stm32f411/regmap.h>
#include <stm32f411/timer.h>

namespace gpio = stm32f411::gpio;
namespace rcc = stm32f411::rcc;

template<typename T>
concept readable = requires(const T& reg) { reg.read(); };
template<typename T>
concept writable = requires(T& reg) { reg.write(0); };
static_assert(readable<gpio::IDR> && !writable<gpio::IDR>);
static_assert(writable<gpio::BSRR> && !readable<gpio::BSRR>);
static_assert(sizeof(gpio::registers_t) == 0x28);
static_assert(offsetof(rcc::registers_t, DCKCFGR) == 0x8C);
static_assert(rcc::dckcfgr::timpre.mask == 0x01000000);
static_assert(rcc::pllcfgr::plln.mask == 0x7FC0);
static_assert(rcc::apb2enr::spi5en.mask == 0x00100000);
static_assert(gpio::metadata::gpioa::moder::reset_value == 0xA8000000);
static_assert(gpio::metadata::gpiob::moder::reset_value == 0x280);
static_assert(gpio::metadata::gpioe::moder::reset_value == 0);
static_assert(gpio::gpioh_address == 0x40021C00);

// The driver-facing pin view must expose the generated fields at both bank edges.
static_assert(gpio::pin_fields<0>::mode.mask == gpio::moder::moder0.mask);
static_assert(gpio::pin_fields<0>::set.mask == gpio::bsrr::bs0.mask);
static_assert(gpio::pin_fields<0>::reset.mask == gpio::bsrr::br0.mask);
static_assert(gpio::pin_fields<7>::alternate_function.mask == gpio::afrl::afrl7.mask);
static_assert(gpio::pin_fields<8>::alternate_function.mask == gpio::afrh::afrh8.mask);
static_assert(gpio::pin_fields<15>::mode.mask == gpio::moder::moder15.mask);
static_assert(gpio::pin_fields<15>::reset.mask == gpio::bsrr::br15.mask);

namespace tim = stm32f411::tim;
namespace usb = stm32f411::usb_otg_fs;
namespace i2c = stm32f411::i2c;

// Instance-specific layouts must not expose advanced timer registers on TIM10.
template<typename T>
concept has_break_control = requires(T& r) { r.BDTR; };
static_assert(has_break_control<tim::tim1::registers_t>);
static_assert(!has_break_control<tim::tim10::registers_t>);
static_assert(tim::tim2::ccr1::ccr1.encode(0x12345678) == 0x12345678);
static_assert(tim::tim5::ccr4::ccr4.value_mask == 0xFFFFFFFF);
static_assert(tim::tim10::ccr1::ccr1.value_mask == 0xFFFF);
static_assert(!readable<tim::tim1::EGR> && writable<tim::tim1::EGR>);
static_assert(offsetof(tim::tim2::registers_t, CCMR1_Input) == 0x18);
static_assert(offsetof(tim::tim2::registers_t, CCMR1_Output) == 0x18);
static_assert(offsetof(tim::tim2::registers_t, CNT) == 0x24);
static_assert(offsetof(tim::tim2::registers_t, OR) == 0x50);
static_assert(offsetof(i2c::registers_t, FLTR) == 0x24);
static_assert(i2c::oar1::add7.mask == 0xFE);
static_assert(!readable<stm32f411::flash::KEYR>);
static_assert(!readable<stm32f411::dma::LIFCR>);
static_assert(usb::otg_fs_device::fs_diepctl0::mpsiz.value_mask == 3);
static_assert(usb::otg_fs_device::diepctl1::mpsiz.value_mask == 0x7FF);
static_assert(usb::otg_fs_device::dieptsiz0::xfrsiz.value_mask == 0x7F);
static_assert(usb::otg_fs_device::dieptsiz1::xfrsiz.value_mask == 0x7FFFF);
static_assert(offsetof(usb::otg_fs_global::registers_t, FS_GRXSTSP_Device) == 0x20);
static_assert(offsetof(usb::otg_fs_global::registers_t, FS_GRXSTSP_Host) == 0x20);
static_assert(offsetof(usb::otg_fs_global::registers_t, FS_GRXFSIZ) == 0x24);

static_assert(stm32f411::timer::timer2.free_running_up_counter().reload == 0xFFFF'FFFFU);
static_assert(stm32f411::timer::timer3.free_running_up_counter().reload == 0xFFFFU);
static_assert(!stm32::timer::calculate_prescaler(artos::hertz_t { 84'000'000 }, artos::hertz_t { 0 }));
static_assert(stm32::timer::calculate_prescaler(artos::hertz_t { 84'000'000 }, artos::hertz_t { 1'000'000 }) == 83);
static_assert(stm32::timer::calculate_prescaler(artos::hertz_t { 1'000 }, artos::hertz_t { 223 })
              == 4); // 200 Hz is closer than 250 Hz.
static_assert(stm32::timer::calculate_prescaler(artos::hertz_t { 84'000'000 }, artos::hertz_t { 1 }) == 65'535);
static_assert(!stm32f411::timer::timer2.free_running_up_counter().tick_frequency);
static_assert(artos::to_hertz(artos::megahertz_t { 84 }) == artos::hertz_t { 84'000'000 });
static_assert(artos::to_hertz(artos::frequency_t<std::uint32_t, std::ratio<1, 2>> { 2 }) == artos::hertz_t { 1 });
static_assert(!artos::to_hertz(artos::frequency_t<std::uint32_t, std::ratio<1, 2>> { 1 }));
static_assert(!artos::to_hertz(artos::megahertz_t { 5'000 }));
