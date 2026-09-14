#pragma once
#include <cstdint>

// External interrupt numbers from this SoC vector table.
namespace artos::irq {
    enum class irq_t : std::uint32_t
    {
        wwdg = 0,
        pvd = 1,
        tamp_stamp = 2,
        rtc_wkup = 3,
        flash = 4,
        rcc = 5,
        exti0 = 6,
        exti1 = 7,
        exti2 = 8,
        exti3 = 9,
        exti4 = 10,
        dma1_stream0 = 11,
        dma1_stream1 = 12,
        dma1_stream2 = 13,
        dma1_stream3 = 14,
        dma1_stream4 = 15,
        dma1_stream5 = 16,
        dma1_stream6 = 17,
        adc = 18,
        can1_tx = 19,
        can1_rx0 = 20,
        can1_rx1 = 21,
        can1_sce = 22,
        exti9_5 = 23,
        tim1_brk_tim9 = 24,
        tim1_up_tim10 = 25,
        tim1_trg_com_tim11 = 26,
        tim1_cc = 27,
        tim2 = 28,
        tim3 = 29,
        tim4 = 30,
        i2c1_ev = 31,
        i2c1_er = 32,
        i2c2_ev = 33,
        i2c2_er = 34,
        spi1 = 35,
        spi2 = 36,
        usart1 = 37,
        usart2 = 38,
        usart3 = 39,
        exti15_10 = 40,
        rtc_alarm = 41,
        otg_fs_wkup = 42,
        tim8_brk_tim12 = 43,
        tim8_up_tim13 = 44,
        tim8_trg_com_tim14 = 45,
        tim8_cc = 46,
        dma1_stream7 = 47,
        fmc = 48,
        sdio = 49,
        tim5 = 50,
        spi3 = 51,
        uart4 = 52,
        uart5 = 53,
        tim6_dac = 54,
        tim7 = 55,
        dma2_stream0 = 56,
        dma2_stream1 = 57,
        dma2_stream2 = 58,
        dma2_stream3 = 59,
        dma2_stream4 = 60,
        eth = 61,
        eth_wkup = 62,
        can2_tx = 63,
        can2_rx0 = 64,
        can2_rx1 = 65,
        can2_sce = 66,
        otg_fs = 67,
        dma2_stream5 = 68,
        dma2_stream6 = 69,
        dma2_stream7 = 70,
        usart6 = 71,
        i2c3_ev = 72,
        i2c3_er = 73,
        otg_hs_ep1_out = 74,
        otg_hs_ep1_in = 75,
        otg_hs_wkup = 76,
        otg_hs = 77,
        dcmi = 78,
        cryp = 79,
        hash_rng = 80,
        fpu = 81,
        uart7 = 82,
        uart8 = 83,
        spi4 = 84,
        spi5 = 85,
        spi6 = 86,
        sai1 = 87,
        ltdc = 88,
        ltdc_er = 89,
        dma2d = 90,
    };
    inline constexpr std::uint32_t irq_count = 91;
    constexpr bool                 is_valid(irq_t number)
    {
        switch (number)
        {
            case irq_t::wwdg:
            case irq_t::pvd:
            case irq_t::tamp_stamp:
            case irq_t::rtc_wkup:
            case irq_t::flash:
            case irq_t::rcc:
            case irq_t::exti0:
            case irq_t::exti1:
            case irq_t::exti2:
            case irq_t::exti3:
            case irq_t::exti4:
            case irq_t::dma1_stream0:
            case irq_t::dma1_stream1:
            case irq_t::dma1_stream2:
            case irq_t::dma1_stream3:
            case irq_t::dma1_stream4:
            case irq_t::dma1_stream5:
            case irq_t::dma1_stream6:
            case irq_t::adc:
            case irq_t::can1_tx:
            case irq_t::can1_rx0:
            case irq_t::can1_rx1:
            case irq_t::can1_sce:
            case irq_t::exti9_5:
            case irq_t::tim1_brk_tim9:
            case irq_t::tim1_up_tim10:
            case irq_t::tim1_trg_com_tim11:
            case irq_t::tim1_cc:
            case irq_t::tim2:
            case irq_t::tim3:
            case irq_t::tim4:
            case irq_t::i2c1_ev:
            case irq_t::i2c1_er:
            case irq_t::i2c2_ev:
            case irq_t::i2c2_er:
            case irq_t::spi1:
            case irq_t::spi2:
            case irq_t::usart1:
            case irq_t::usart2:
            case irq_t::usart3:
            case irq_t::exti15_10:
            case irq_t::rtc_alarm:
            case irq_t::otg_fs_wkup:
            case irq_t::tim8_brk_tim12:
            case irq_t::tim8_up_tim13:
            case irq_t::tim8_trg_com_tim14:
            case irq_t::tim8_cc:
            case irq_t::dma1_stream7:
            case irq_t::fmc:
            case irq_t::sdio:
            case irq_t::tim5:
            case irq_t::spi3:
            case irq_t::uart4:
            case irq_t::uart5:
            case irq_t::tim6_dac:
            case irq_t::tim7:
            case irq_t::dma2_stream0:
            case irq_t::dma2_stream1:
            case irq_t::dma2_stream2:
            case irq_t::dma2_stream3:
            case irq_t::dma2_stream4:
            case irq_t::eth:
            case irq_t::eth_wkup:
            case irq_t::can2_tx:
            case irq_t::can2_rx0:
            case irq_t::can2_rx1:
            case irq_t::can2_sce:
            case irq_t::otg_fs:
            case irq_t::dma2_stream5:
            case irq_t::dma2_stream6:
            case irq_t::dma2_stream7:
            case irq_t::usart6:
            case irq_t::i2c3_ev:
            case irq_t::i2c3_er:
            case irq_t::otg_hs_ep1_out:
            case irq_t::otg_hs_ep1_in:
            case irq_t::otg_hs_wkup:
            case irq_t::otg_hs:
            case irq_t::dcmi:
            case irq_t::cryp:
            case irq_t::hash_rng:
            case irq_t::fpu:
            case irq_t::uart7:
            case irq_t::uart8:
            case irq_t::spi4:
            case irq_t::spi5:
            case irq_t::spi6:
            case irq_t::sai1:
            case irq_t::ltdc:
            case irq_t::ltdc_er:
            case irq_t::dma2d:
                return true;
            default:
                return false;
        }
    }
}
