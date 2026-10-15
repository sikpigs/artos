#pragma once

#include <cstddef>
#include <cstdint>
#include <mmio.h>

// Core register map for thread switching, FPU context preservation, and the
// Armv7-M MPU.
// Reference: ST PM0214, sections 4.4, 4.6, and 4.7 (SCB, FPU, and MPU).
// https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortex-m4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf
// PSP, MSP, CONTROL and BASEPRI are CPU registers, accessed with MRS/MSR.
namespace artos::port::regmap {
    namespace scb {
        namespace tags {
            struct icsr;
            struct shcsr;
            struct cfsr;
            struct hfsr;
            struct dfsr;
            struct mmfar;
            struct bfar;
            struct afsr;
            struct shpr2;
            struct shpr3;
            struct cpacr;
        }

        // ICSR has write-one actions. Inherit direct writes, never RMW or clear().
        // Readback is available for diagnostics; do not write that snapshot back.
        struct ICSR : artos::mmio::write_only_register_t<tags::icsr>
        {
            [[nodiscard]] std::uint32_t read() const noexcept
            {
                return value;
            }
        };

        using SHPR2 = artos::mmio::register_t<tags::shpr2>;
        using SHPR3 = artos::mmio::register_t<tags::shpr3>;
        using CPACR = artos::mmio::register_t<tags::cpacr>;
        using SHCSR = artos::mmio::register_t<tags::shcsr>;
        using MMFAR = artos::mmio::register_t<tags::mmfar>;
        using BFAR = artos::mmio::register_t<tags::bfar>;
        using AFSR = artos::mmio::register_t<tags::afsr>;

        template<typename Tag>
        struct write_one_to_clear_register_t : artos::mmio::write_only_register_t<Tag>
        {
            [[nodiscard]] std::uint32_t read() const noexcept
            {
                return this->value;
            }
        };

        using CFSR = write_one_to_clear_register_t<tags::cfsr>;
        using HFSR = write_one_to_clear_register_t<tags::hfsr>;
        using DFSR = write_one_to_clear_register_t<tags::dfsr>;

        namespace icsr {
            inline constexpr artos::mmio::field_t<tags::icsr, 25> pendstclr {};
            inline constexpr artos::mmio::field_t<tags::icsr, 26> pendstset {};
            inline constexpr artos::mmio::field_t<tags::icsr, 27> pendsvclr {};
            inline constexpr artos::mmio::field_t<tags::icsr, 28> pendsvset {};
            // Set and clear actions for the same exception must not be combined.
        }

        // Raw encoded priority bytes, not logical priority numbers. Implemented
        // priority bits occupy the high bits (on STM32F411, use priority << 4).
        namespace shpr2 {
            inline constexpr artos::mmio::field_t<tags::shpr2, 24, 8> svc {};
        }

        namespace shpr3 {
            inline constexpr artos::mmio::field_t<tags::shpr3, 16, 8> pendsv {};
            inline constexpr artos::mmio::field_t<tags::shpr3, 24, 8> systick {};
        }

        namespace shcsr {
            inline constexpr artos::mmio::field_t<tags::shcsr, 0>  memfaultact {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 1>  busfaultact {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 3>  usgfaultact {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 7>  svcallact {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 8>  monitoract {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 10> pendsvact {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 11> systickact {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 12> usgfaultpended {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 13> memfaultpended {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 14> busfaultpended {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 15> svcallpended {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 16> memfaultena {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 17> busfaultena {};
            inline constexpr artos::mmio::field_t<tags::shcsr, 18> usgfaultena {};
        }

        namespace cfsr {
            // Snapshot with read(); acknowledge selected flags by writing ones.
            // The three status groups share this 32-bit register.
            inline constexpr artos::mmio::field_t<tags::cfsr, 0, 8>   memfaultsr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 8, 8>   busfaultsr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 16, 16> usgfaultsr {};

            // MemManage fault status (MMFSR).
            inline constexpr artos::mmio::field_t<tags::cfsr, 0> iaccviol {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 1> daccviol {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 3> munstkerr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 4> mstkerr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 5> mlsperr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 7> mmarvalid {};

            // BusFault status (BFSR).
            inline constexpr artos::mmio::field_t<tags::cfsr, 8>  ibuserr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 9>  preciserr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 10> impreciserr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 11> unstkerr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 12> stkerr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 13> lsperr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 15> bfarvalid {};

            // UsageFault status (UFSR).
            inline constexpr artos::mmio::field_t<tags::cfsr, 16> undefinstr {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 17> invstate {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 18> invpc {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 19> nocp {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 24> unaligned {};
            inline constexpr artos::mmio::field_t<tags::cfsr, 25> divbyzero {};
        }

        namespace cpacr {
            inline constexpr artos::mmio::field_t<tags::cpacr, 20, 2> cp10 {};
            inline constexpr artos::mmio::field_t<tags::cpacr, 22, 2> cp11 {};
            inline constexpr std::uint32_t                            full_access = 3;
            // Set CP10 and CP11 identically. Follow access changes with DSB/ISB
            // before executing instructions that depend on the new permissions.
        }

        // NOLINTBEGIN(readability-identifier-naming)
        struct registers_t
        {
            std::uint32_t reserved_x00;
            scb::ICSR     ICSR;
            std::uint32_t reserved_x08_x18[5];
            scb::SHPR2    SHPR2;
            scb::SHPR3    SHPR3;
            scb::SHCSR    SHCSR;
            scb::CFSR     CFSR;
            scb::HFSR     HFSR;
            scb::DFSR     DFSR;
            scb::MMFAR    MMFAR;
            scb::BFAR     BFAR;
            scb::AFSR     AFSR;
            std::uint32_t reserved_x40_x84[18];
            scb::CPACR    CPACR;
        };

        // NOLINTEND(readability-identifier-naming)

        static_assert(sizeof(ICSR) == sizeof(std::uint32_t));
        static_assert(alignof(registers_t) == alignof(std::uint32_t));
        static_assert(offsetof(registers_t, ICSR) == 0x04);
        static_assert(offsetof(registers_t, SHPR2) == 0x1C);
        static_assert(offsetof(registers_t, SHPR3) == 0x20);
        static_assert(offsetof(registers_t, SHCSR) == 0x24);
        static_assert(offsetof(registers_t, CFSR) == 0x28);
        static_assert(offsetof(registers_t, HFSR) == 0x2C);
        static_assert(offsetof(registers_t, DFSR) == 0x30);
        static_assert(offsetof(registers_t, MMFAR) == 0x34);
        static_assert(offsetof(registers_t, BFAR) == 0x38);
        static_assert(offsetof(registers_t, AFSR) == 0x3C);
        static_assert(offsetof(registers_t, CPACR) == 0x88);
        static_assert(sizeof(registers_t) == 0x8C);
    }

    namespace systick {
        namespace tags {
            struct ctrl;
            struct load;
            struct val;
        }

        using CTRL = artos::mmio::register_t<tags::ctrl>;
        using LOAD = artos::mmio::register_t<tags::load>;
        using VAL = artos::mmio::register_t<tags::val>;

        namespace ctrl {
            inline constexpr artos::mmio::field_t<tags::ctrl, 0>  enable {};
            inline constexpr artos::mmio::field_t<tags::ctrl, 1>  tickint {};
            inline constexpr artos::mmio::field_t<tags::ctrl, 2>  clocksource {};
            inline constexpr artos::mmio::field_t<tags::ctrl, 16> countflag {}; // NOLINT
        }

        namespace load {
            inline constexpr artos::mmio::field_t<tags::load, 0, 24> load {}; // NOLINT
        }

        namespace val {
            inline constexpr artos::mmio::field_t<tags::val, 0, 24> val {}; // NOLINT
        }

        // Preserve the architectural register names from the Cortex-M map.
        // NOLINTBEGIN(readability-identifier-naming)
        struct registers_t
        {
            systick::CTRL CTRL;
            systick::LOAD LOAD;
            systick::VAL  VAL;
        };

        // NOLINTEND(readability-identifier-naming)

        static_assert(alignof(registers_t) == alignof(std::uint32_t));
        static_assert(offsetof(registers_t, CTRL) == 0x00);
        static_assert(offsetof(registers_t, LOAD) == 0x04);
        static_assert(offsetof(registers_t, VAL) == 0x08);
        static_assert(sizeof(registers_t) == 0x0C);

        inline registers_t* at(std::uintptr_t address) noexcept
        {
            return reinterpret_cast<registers_t*>(address);
        }
    }

    namespace core_debug {
        namespace tags {
            struct dhcsr;
        }

        using DHCSR = artos::mmio::read_only_register_t<tags::dhcsr>;

        namespace dhcsr {
            inline constexpr artos::mmio::field_t<tags::dhcsr, 0> debug_enable {};
        }

        // NOLINTBEGIN(readability-identifier-naming)
        struct registers_t
        {
            core_debug::DHCSR DHCSR;
        };

        // NOLINTEND(readability-identifier-naming)

        static_assert(offsetof(registers_t, DHCSR) == 0x00);
        static_assert(sizeof(registers_t) == 0x04);
    }

    namespace mpu {
        namespace tags {
            struct type;
            struct ctrl;
            struct rnr;
            struct rbar;
            struct rasr;
        }

        using TYPE = artos::mmio::read_only_register_t<tags::type>;
        using CTRL = artos::mmio::register_t<tags::ctrl>;
        using RNR = artos::mmio::register_t<tags::rnr>;
        using RBAR = artos::mmio::register_t<tags::rbar>;
        using RASR = artos::mmio::register_t<tags::rasr>;

        namespace type {
            inline constexpr artos::mmio::field_t<tags::type, 0, 8> iregion {};
            inline constexpr artos::mmio::field_t<tags::type, 8, 8> dregion {};
            inline constexpr artos::mmio::field_t<tags::type, 0>    separate {};
        }

        namespace ctrl {
            inline constexpr artos::mmio::field_t<tags::ctrl, 0> enable {};
            inline constexpr artos::mmio::field_t<tags::ctrl, 1> hfnmiena {};
            inline constexpr artos::mmio::field_t<tags::ctrl, 2> privdefena {};
        }

        namespace rnr {
            inline constexpr artos::mmio::field_t<tags::rnr, 0, 8> region {};
        }

        namespace rbar {
            // The field value is the aligned base address shifted right by 5.
            inline constexpr artos::mmio::field_t<tags::rbar, 5, 27> address {};
            inline constexpr artos::mmio::field_t<tags::rbar, 4>     valid {};
            inline constexpr artos::mmio::field_t<tags::rbar, 0, 4>  region {};
        }

        namespace rasr {
            inline constexpr artos::mmio::field_t<tags::rasr, 0> enable {};
            // SIZE encodes log2(region bytes) - 1.
            inline constexpr artos::mmio::field_t<tags::rasr, 1, 5>  size {};
            inline constexpr artos::mmio::field_t<tags::rasr, 8, 8>  subregion_disable {};
            inline constexpr artos::mmio::field_t<tags::rasr, 16>    bufferable {};
            inline constexpr artos::mmio::field_t<tags::rasr, 17>    cacheable {};
            inline constexpr artos::mmio::field_t<tags::rasr, 18>    shareable {};
            inline constexpr artos::mmio::field_t<tags::rasr, 19, 3> tex {};
            inline constexpr artos::mmio::field_t<tags::rasr, 24, 3> ap {};
            inline constexpr artos::mmio::field_t<tags::rasr, 28>    xn {};
            inline constexpr artos::mmio::field_t<tags::rasr, 29, 3> reserved {};
        }

        // Preserve the architectural register names from the Armv7-M MPU map.
        // NOLINTBEGIN(readability-identifier-naming)
        struct registers_t
        {
            mpu::TYPE TYPE;
            mpu::CTRL CTRL;
            mpu::RNR  RNR;
            mpu::RBAR RBAR;
            mpu::RASR RASR;
        };

        // NOLINTEND(readability-identifier-naming)

        static_assert(alignof(registers_t) == alignof(std::uint32_t));
        static_assert(offsetof(registers_t, TYPE) == 0x00);
        static_assert(offsetof(registers_t, CTRL) == 0x04);
        static_assert(offsetof(registers_t, RNR) == 0x08);
        static_assert(offsetof(registers_t, RBAR) == 0x0C);
        static_assert(offsetof(registers_t, RASR) == 0x10);
        static_assert(sizeof(registers_t) == 0x14);
    }

    namespace fpu {
        namespace tags {
            struct fpccr;
        }

        using FPCCR = artos::mmio::register_t<tags::fpccr>;

        namespace fpccr {
            inline constexpr artos::mmio::field_t<tags::fpccr, 30> lspen {};
            inline constexpr artos::mmio::field_t<tags::fpccr, 31> aspen {};
            // Configure before starting threads; do not change preservation
            // policy while a lazy FP save is pending.
        }

        // Preserve the architectural register names from the Armv7-M MPU map.
        // NOLINTBEGIN(readability-identifier-naming)
        struct registers_t
        {
            std::uint32_t reserved_x00;
            fpu::FPCCR    FPCCR;
        };

        // NOLINTEND(readability-identifier-naming)

        static_assert(alignof(registers_t) == alignof(std::uint32_t));
        static_assert(offsetof(registers_t, FPCCR) == 0x04);
        static_assert(sizeof(registers_t) == 0x08);
    }

    inline scb::registers_t* const        SCB = reinterpret_cast<scb::registers_t*>(0xE000ED00UL);
    inline mpu::registers_t* const        MPU = reinterpret_cast<mpu::registers_t*>(0xE000ED90UL);
    inline core_debug::registers_t* const DEBUG = reinterpret_cast<core_debug::registers_t*>(0xE000EDF0UL);
    inline fpu::registers_t* const        FPU = reinterpret_cast<fpu::registers_t*>(0xE000EF30UL);

    // Usage:
    // SCB->ICSR.set(scb::icsr::pendsvset);  // Direct write of bit 28.
    // SCB->SHPR3.write(scb::shpr3::pendsv(0xF0));
    // FPU->FPCCR.set(fpu::fpccr::aspen, fpu::fpccr::lspen);
}
