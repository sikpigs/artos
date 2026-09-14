#pragma once

#include <cstddef>
#include <cstdint>
#include <mmio.h>

// Minimal core register map for thread switching and FPU context preservation.
// Reference: ST PM0214, sections 4.4 and 4.6 (SCB and FPU).
// https://www.st.com/resource/en/programming_manual/dm00046982-stm32-cortex-m4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf
// PSP, MSP, CONTROL and BASEPRI are CPU registers, accessed with MRS/MSR.
namespace artos::port::regmap {
    namespace scb {
        namespace tags {
            struct icsr;
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

        namespace cpacr {
            inline constexpr artos::mmio::field_t<tags::cpacr, 20, 2> cp10 {};
            inline constexpr artos::mmio::field_t<tags::cpacr, 22, 2> cp11 {};
            inline constexpr std::uint32_t                            full_access = 3;
            // Set CP10 and CP11 identically. Follow access changes with DSB/ISB
            // before executing instructions that depend on the new permissions.
        }

        struct registers_t
        {
            std::uint32_t reserved_x00;
            scb::ICSR     ICSR;
            std::uint32_t reserved_x08_x18[5];
            scb::SHPR2    SHPR2;
            scb::SHPR3    SHPR3;
            std::uint32_t reserved_x24_x84[25];
            scb::CPACR    CPACR;
        };

        static_assert(sizeof(ICSR) == sizeof(std::uint32_t));
        static_assert(alignof(registers_t) == alignof(std::uint32_t));
        static_assert(offsetof(registers_t, ICSR) == 0x04);
        static_assert(offsetof(registers_t, SHPR2) == 0x1C);
        static_assert(offsetof(registers_t, SHPR3) == 0x20);
        static_assert(offsetof(registers_t, CPACR) == 0x88);
        static_assert(sizeof(registers_t) == 0x8C);
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

        struct registers_t
        {
            std::uint32_t reserved_x00;
            fpu::FPCCR    FPCCR;
        };

        static_assert(alignof(registers_t) == alignof(std::uint32_t));
        static_assert(offsetof(registers_t, FPCCR) == 0x04);
        static_assert(sizeof(registers_t) == 0x08);
    }

    inline scb::registers_t* const SCB = reinterpret_cast<scb::registers_t*>(0xE000ED00UL);
    inline fpu::registers_t* const FPU = reinterpret_cast<fpu::registers_t*>(0xE000EF30UL);

    // Usage:
    // SCB->ICSR.set(scb::icsr::pendsvset);  // Direct write of bit 28.
    // SCB->SHPR3.write(scb::shpr3::pendsv(0xF0));
    // FPU->FPCCR.set(fpu::fpccr::aspen, fpu::fpccr::lspen);
}
