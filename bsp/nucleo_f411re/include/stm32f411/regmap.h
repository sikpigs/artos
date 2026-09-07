#pragma once

#include <mmio.h>
#include <irq.h>

namespace stm32f411::rcc {
    inline constexpr std::uint32_t address = 0x40023800UL;

    namespace tags {
        struct cr;
        struct pllcfgr;
        struct cfgr;
        struct cir;
        struct ahb1rstr;
        struct ahb2rstr;
        struct apb1rstr;
        struct apb2rstr;
        struct ahb1enr;
        struct ahb2enr;
        struct apb1enr;
        struct apb2enr;
        struct ahb1lpenr;
        struct ahb2lpenr;
        struct apb1lpenr;
        struct apb2lpenr;
        struct bdcr;
        struct csr;
        struct sscgr;
        struct plli2scfgr;
        struct dckcfgr;
    }

    using CR = register_t<tags::cr>;
    using PLLCFGR = register_t<tags::pllcfgr>;
    using CFGR = register_t<tags::cfgr>;
    using CIR = register_t<tags::cir>;
    using AHB1RSTR = register_t<tags::ahb1rstr>;
    using AHB2RSTR = register_t<tags::ahb2rstr>;
    using APB1RSTR = register_t<tags::apb1rstr>;
    using APB2RSTR = register_t<tags::apb2rstr>;
    using AHB1ENR = register_t<tags::ahb1enr>;
    using AHB2ENR = register_t<tags::ahb2enr>;
    using APB1ENR = register_t<tags::apb1enr>;
    using APB2ENR = register_t<tags::apb2enr>;
    using AHB1LPENR = register_t<tags::ahb1lpenr>;
    using AHB2LPENR = register_t<tags::ahb2lpenr>;
    using APB1LPENR = register_t<tags::apb1lpenr>;
    using APB2LPENR = register_t<tags::apb2lpenr>;
    using BDCR = register_t<tags::bdcr>;
    using CSR = register_t<tags::csr>;
    using SSCGR = register_t<tags::sscgr>;
    using PLLI2SCFGR = register_t<tags::plli2scfgr>;
    using DCKCFGR = register_t<tags::dckcfgr>;

    namespace cr {
        inline constexpr field_t<tags::cr, 0>    hsion {};
        inline constexpr field_t<tags::cr, 1>    hsirdy {};
        inline constexpr field_t<tags::cr, 3, 5> hsitrim {};
        inline constexpr field_t<tags::cr, 8, 8> hsical {};
        inline constexpr field_t<tags::cr, 16>   hseon {};
        inline constexpr field_t<tags::cr, 17>   hserdy {};
        inline constexpr field_t<tags::cr, 18>   hsebyp {};
        inline constexpr field_t<tags::cr, 19>   csson {};
        inline constexpr field_t<tags::cr, 24>   pllon {};
        inline constexpr field_t<tags::cr, 25>   pllrdy {};
        inline constexpr field_t<tags::cr, 26>   plli2son {};
        inline constexpr field_t<tags::cr, 27>   plli2srdy {};
    }

    namespace pllcfgr {
        inline constexpr field_t<tags::pllcfgr, 0, 6>  pllm {};
        inline constexpr field_t<tags::pllcfgr, 6, 9>  plln {};
        inline constexpr field_t<tags::pllcfgr, 16, 2> pllp {};
        inline constexpr field_t<tags::pllcfgr, 22>    pllsrc {};
        inline constexpr field_t<tags::pllcfgr, 24, 4> pllq {};
    }

    namespace cfgr {
        inline constexpr field_t<tags::cfgr, 0, 2>  sw {};
        inline constexpr field_t<tags::cfgr, 2, 2>  sws {};
        inline constexpr field_t<tags::cfgr, 4, 4>  hpre {};
        inline constexpr field_t<tags::cfgr, 10, 3> ppre1 {};
        inline constexpr field_t<tags::cfgr, 13, 3> ppre2 {};
        inline constexpr field_t<tags::cfgr, 16, 5> rtcpre {};
        inline constexpr field_t<tags::cfgr, 21, 2> mco1 {};
        inline constexpr field_t<tags::cfgr, 23>    i2ssrc {};
        inline constexpr field_t<tags::cfgr, 24, 3> mco1pre {};
        inline constexpr field_t<tags::cfgr, 27, 3> mco2pre {};
        inline constexpr field_t<tags::cfgr, 30, 2> mco2 {};
    }

    namespace cir {
        inline constexpr field_t<tags::cir, 0>  lsirdyf {};
        inline constexpr field_t<tags::cir, 1>  lserdyf {};
        inline constexpr field_t<tags::cir, 2>  hsirdyf {};
        inline constexpr field_t<tags::cir, 3>  hserdyf {};
        inline constexpr field_t<tags::cir, 4>  pllrdyf {};
        inline constexpr field_t<tags::cir, 5>  plli2srdyf {};
        inline constexpr field_t<tags::cir, 7>  cssf {};
        inline constexpr field_t<tags::cir, 8>  lsirdyie {};
        inline constexpr field_t<tags::cir, 9>  lserdyie {};
        inline constexpr field_t<tags::cir, 10> hsirdyie {};
        inline constexpr field_t<tags::cir, 11> hserdyie {};
        inline constexpr field_t<tags::cir, 12> pllrdyie {};
        inline constexpr field_t<tags::cir, 13> plli2srdyie {};
        inline constexpr field_t<tags::cir, 16> lsirdyc {};
        inline constexpr field_t<tags::cir, 17> lserdyc {};
        inline constexpr field_t<tags::cir, 18> hsirdyc {};
        inline constexpr field_t<tags::cir, 19> hserdyc {};
        inline constexpr field_t<tags::cir, 20> pllrdyc {};
        inline constexpr field_t<tags::cir, 21> plli2srdyc {};
        inline constexpr field_t<tags::cir, 23> cssc {};
    }

    namespace ahb1rstr {
        inline constexpr field_t<tags::ahb1rstr, 0>  gpioarst {};
        inline constexpr field_t<tags::ahb1rstr, 1>  gpiobrst {};
        inline constexpr field_t<tags::ahb1rstr, 2>  gpiocrst {};
        inline constexpr field_t<tags::ahb1rstr, 3>  gpiodrst {};
        inline constexpr field_t<tags::ahb1rstr, 4>  gpioerst {};
        inline constexpr field_t<tags::ahb1rstr, 7>  gpiohrst {};
        inline constexpr field_t<tags::ahb1rstr, 12> crcrst {};
        inline constexpr field_t<tags::ahb1rstr, 21> dma1rst {};
        inline constexpr field_t<tags::ahb1rstr, 22> dma2rst {};
    }

    namespace ahb2rstr {
        inline constexpr field_t<tags::ahb2rstr, 7> otgfsrst {};
    }

    namespace apb1rstr {
        inline constexpr field_t<tags::apb1rstr, 0>  tim2rst {};
        inline constexpr field_t<tags::apb1rstr, 1>  tim3rst {};
        inline constexpr field_t<tags::apb1rstr, 2>  tim4rst {};
        inline constexpr field_t<tags::apb1rstr, 3>  tim5rst {};
        inline constexpr field_t<tags::apb1rstr, 11> wwdgrst {};
        inline constexpr field_t<tags::apb1rstr, 14> spi2rst {};
        inline constexpr field_t<tags::apb1rstr, 15> spi3rst {};
        inline constexpr field_t<tags::apb1rstr, 17> usart2rst {};
        inline constexpr field_t<tags::apb1rstr, 21> i2c1rst {};
        inline constexpr field_t<tags::apb1rstr, 22> i2c2rst {};
        inline constexpr field_t<tags::apb1rstr, 23> i2c3rst {};
        inline constexpr field_t<tags::apb1rstr, 28> pwrrst {};
    }

    namespace apb2rstr {
        inline constexpr field_t<tags::apb2rstr, 0>  tim1rst {};
        inline constexpr field_t<tags::apb2rstr, 4>  usart1rst {};
        inline constexpr field_t<tags::apb2rstr, 5>  usart6rst {};
        inline constexpr field_t<tags::apb2rstr, 8>  adcrst {};
        inline constexpr field_t<tags::apb2rstr, 11> sdiorst {};
        inline constexpr field_t<tags::apb2rstr, 12> spi1rst {};
        inline constexpr field_t<tags::apb2rstr, 13> spi4rst {};
        inline constexpr field_t<tags::apb2rstr, 14> syscfgrst {};
        inline constexpr field_t<tags::apb2rstr, 16> tim9rst {};
        inline constexpr field_t<tags::apb2rstr, 17> tim10rst {};
        inline constexpr field_t<tags::apb2rstr, 18> tim11rst {};
        inline constexpr field_t<tags::apb2rstr, 20> spi5rst {};
    }

    namespace ahb1enr {
        inline constexpr field_t<tags::ahb1enr, 0>  gpioaen {};
        inline constexpr field_t<tags::ahb1enr, 1>  gpioben {};
        inline constexpr field_t<tags::ahb1enr, 2>  gpiocen {};
        inline constexpr field_t<tags::ahb1enr, 3>  gpioden {};
        inline constexpr field_t<tags::ahb1enr, 4>  gpioeen {};
        inline constexpr field_t<tags::ahb1enr, 7>  gpiohen {};
        inline constexpr field_t<tags::ahb1enr, 12> crcen {};
        inline constexpr field_t<tags::ahb1enr, 21> dma1en {};
        inline constexpr field_t<tags::ahb1enr, 22> dma2en {};
    }

    namespace ahb2enr {
        inline constexpr field_t<tags::ahb2enr, 7> otgfsen {};
    }

    namespace apb1enr {
        inline constexpr field_t<tags::apb1enr, 0>  tim2en {};
        inline constexpr field_t<tags::apb1enr, 1>  tim3en {};
        inline constexpr field_t<tags::apb1enr, 2>  tim4en {};
        inline constexpr field_t<tags::apb1enr, 3>  tim5en {};
        inline constexpr field_t<tags::apb1enr, 11> wwdgen {};
        inline constexpr field_t<tags::apb1enr, 14> spi2en {};
        inline constexpr field_t<tags::apb1enr, 15> spi3en {};
        inline constexpr field_t<tags::apb1enr, 17> usart2en {};
        inline constexpr field_t<tags::apb1enr, 21> i2c1en {};
        inline constexpr field_t<tags::apb1enr, 22> i2c2en {};
        inline constexpr field_t<tags::apb1enr, 23> i2c3en {};
        inline constexpr field_t<tags::apb1enr, 28> pwren {};
    }

    namespace apb2enr {
        inline constexpr field_t<tags::apb2enr, 0>  tim1en {};
        inline constexpr field_t<tags::apb2enr, 4>  usart1en {};
        inline constexpr field_t<tags::apb2enr, 5>  usart6en {};
        inline constexpr field_t<tags::apb2enr, 8>  adc1en {};
        inline constexpr field_t<tags::apb2enr, 11> sdioen {};
        inline constexpr field_t<tags::apb2enr, 12> spi1en {};
        inline constexpr field_t<tags::apb2enr, 13> spi4en {};
        inline constexpr field_t<tags::apb2enr, 14> syscfgen {};
        inline constexpr field_t<tags::apb2enr, 16> tim9en {};
        inline constexpr field_t<tags::apb2enr, 17> tim10en {};
        inline constexpr field_t<tags::apb2enr, 18> tim11en {};
        inline constexpr field_t<tags::apb2enr, 20> spi5en {};
    }

    namespace ahb1lpenr {
        inline constexpr field_t<tags::ahb1lpenr, 0>  gpioalpen {};
        inline constexpr field_t<tags::ahb1lpenr, 1>  gpioblpen {};
        inline constexpr field_t<tags::ahb1lpenr, 2>  gpioclpen {};
        inline constexpr field_t<tags::ahb1lpenr, 3>  gpiodlpen {};
        inline constexpr field_t<tags::ahb1lpenr, 4>  gpioelpen {};
        inline constexpr field_t<tags::ahb1lpenr, 7>  gpiohlpen {};
        inline constexpr field_t<tags::ahb1lpenr, 12> crclpen {};
        inline constexpr field_t<tags::ahb1lpenr, 15> flitflpen {};
        inline constexpr field_t<tags::ahb1lpenr, 16> sram1lpen {};
        inline constexpr field_t<tags::ahb1lpenr, 21> dma1lpen {};
        inline constexpr field_t<tags::ahb1lpenr, 22> dma2lpen {};
    }

    namespace ahb2lpenr {
        inline constexpr field_t<tags::ahb2lpenr, 7> otgfslpen {};
    }

    namespace apb1lpenr {
        inline constexpr field_t<tags::apb1lpenr, 0>  tim2lpen {};
        inline constexpr field_t<tags::apb1lpenr, 1>  tim3lpen {};
        inline constexpr field_t<tags::apb1lpenr, 2>  tim4lpen {};
        inline constexpr field_t<tags::apb1lpenr, 3>  tim5lpen {};
        inline constexpr field_t<tags::apb1lpenr, 11> wwdglpen {};
        inline constexpr field_t<tags::apb1lpenr, 14> spi2lpen {};
        inline constexpr field_t<tags::apb1lpenr, 15> spi3lpen {};
        inline constexpr field_t<tags::apb1lpenr, 17> usart2lpen {};
        inline constexpr field_t<tags::apb1lpenr, 21> i2c1lpen {};
        inline constexpr field_t<tags::apb1lpenr, 22> i2c2lpen {};
        inline constexpr field_t<tags::apb1lpenr, 23> i2c3lpen {};
        inline constexpr field_t<tags::apb1lpenr, 28> pwrlpen {};
    }

    namespace apb2lpenr {
        inline constexpr field_t<tags::apb2lpenr, 0>  tim1lpen {};
        inline constexpr field_t<tags::apb2lpenr, 4>  usart1lpen {};
        inline constexpr field_t<tags::apb2lpenr, 5>  usart6lpen {};
        inline constexpr field_t<tags::apb2lpenr, 8>  adc1lpen {};
        inline constexpr field_t<tags::apb2lpenr, 11> sdiolpen {};
        inline constexpr field_t<tags::apb2lpenr, 12> spi1lpen {};
        inline constexpr field_t<tags::apb2lpenr, 13> spi4lpen {};
        inline constexpr field_t<tags::apb2lpenr, 14> syscfglpen {};
        inline constexpr field_t<tags::apb2lpenr, 16> tim9lpen {};
        inline constexpr field_t<tags::apb2lpenr, 17> tim10lpen {};
        inline constexpr field_t<tags::apb2lpenr, 18> tim11lpen {};
        inline constexpr field_t<tags::apb2lpenr, 20> spi5lpen {};
    }

    namespace bdcr {
        inline constexpr field_t<tags::bdcr, 0>    lseon {};
        inline constexpr field_t<tags::bdcr, 1>    lserdy {};
        inline constexpr field_t<tags::bdcr, 2>    lsebyp {};
        inline constexpr field_t<tags::bdcr, 3>    lsemod {};
        inline constexpr field_t<tags::bdcr, 8, 2> rtcsel {};
        inline constexpr field_t<tags::bdcr, 15>   rtcen {};
        inline constexpr field_t<tags::bdcr, 16>   bdrst {};
    }

    namespace csr {
        inline constexpr field_t<tags::csr, 0>  lsion {};
        inline constexpr field_t<tags::csr, 1>  lsirdy {};
        inline constexpr field_t<tags::csr, 24> rmvf {};
        inline constexpr field_t<tags::csr, 25> borrstf {};
        inline constexpr field_t<tags::csr, 26> pinrstf {};
        inline constexpr field_t<tags::csr, 27> porrstf {};
        inline constexpr field_t<tags::csr, 28> sftrstf {};
        inline constexpr field_t<tags::csr, 29> iwdgrstf {};
        inline constexpr field_t<tags::csr, 30> wwdgrstf {};
        inline constexpr field_t<tags::csr, 31> lpwrrstf {};
    }

    namespace sscgr {
        inline constexpr field_t<tags::sscgr, 0, 13>  modper {};
        inline constexpr field_t<tags::sscgr, 13, 15> incstep {};
        inline constexpr field_t<tags::sscgr, 30>     spreadsel {};
        inline constexpr field_t<tags::sscgr, 31>     sscgen {};
    }

    namespace plli2scfgr {
        inline constexpr field_t<tags::plli2scfgr, 0, 6>  plli2sm {};
        inline constexpr field_t<tags::plli2scfgr, 6, 9>  plli2sn {};
        inline constexpr field_t<tags::plli2scfgr, 28, 3> plli2sr {};
    }

    namespace dckcfgr {
        inline constexpr field_t<tags::dckcfgr, 24> timpre {};
    }

    struct registers_t
    {
        rcc::CR         CR;
        rcc::PLLCFGR    PLLCFGR;
        rcc::CFGR       CFGR;
        rcc::CIR        CIR;
        rcc::AHB1RSTR   AHB1RSTR;
        rcc::AHB2RSTR   AHB2RSTR;
        std::uint32_t   reserved_x18_x1C[2];
        rcc::APB1RSTR   APB1RSTR;
        rcc::APB2RSTR   APB2RSTR;
        std::uint32_t   reserved_x28_x2C[2];
        rcc::AHB1ENR    AHB1ENR;
        rcc::AHB2ENR    AHB2ENR;
        std::uint32_t   reserved_x38_x3C[2];
        rcc::APB1ENR    APB1ENR;
        rcc::APB2ENR    APB2ENR;
        std::uint32_t   reserved_x48_x4C[2];
        rcc::AHB1LPENR  AHB1LPENR;
        rcc::AHB2LPENR  AHB2LPENR;
        std::uint32_t   reserved_x58_x5C[2];
        rcc::APB1LPENR  APB1LPENR;
        rcc::APB2LPENR  APB2LPENR;
        std::uint32_t   reserved_x68_x6C[2];
        rcc::BDCR       BDCR;
        rcc::CSR        CSR;
        std::uint32_t   reserved_x78_7C[2];
        rcc::SSCGR      SSCGR;
        rcc::PLLI2SCFGR PLLI2SCFGR;
        std::uint32_t   reserved_x88;
        rcc::DCKCFGR    DCKCFGR;
    };

    static_assert(alignof(registers_t) == alignof(std::uint32_t));
    static_assert(offsetof(registers_t, CR) == 0x00);
    static_assert(offsetof(registers_t, PLLCFGR) == 0x04);
    static_assert(offsetof(registers_t, CFGR) == 0x08);
    static_assert(offsetof(registers_t, CIR) == 0x0C);
    static_assert(offsetof(registers_t, AHB1RSTR) == 0x10);
    static_assert(offsetof(registers_t, AHB2RSTR) == 0x14);
    static_assert(offsetof(registers_t, APB1RSTR) == 0x20);
    static_assert(offsetof(registers_t, APB2RSTR) == 0x24);
    static_assert(offsetof(registers_t, AHB1ENR) == 0x30);
    static_assert(offsetof(registers_t, AHB2ENR) == 0x34);
    static_assert(offsetof(registers_t, APB1ENR) == 0x40);
    static_assert(offsetof(registers_t, APB2ENR) == 0x44);
    static_assert(offsetof(registers_t, AHB1LPENR) == 0x50);
    static_assert(offsetof(registers_t, AHB2LPENR) == 0x54);
    static_assert(offsetof(registers_t, APB1LPENR) == 0x60);
    static_assert(offsetof(registers_t, APB2LPENR) == 0x64);
    static_assert(offsetof(registers_t, BDCR) == 0x70);
    static_assert(offsetof(registers_t, CSR) == 0x74);
    static_assert(offsetof(registers_t, SSCGR) == 0x80);
    static_assert(offsetof(registers_t, PLLI2SCFGR) == 0x84);
    static_assert(offsetof(registers_t, DCKCFGR) == 0x8C);
    static_assert(sizeof(registers_t) == 0x90);

    registers_t* const RCC = reinterpret_cast<registers_t*>(address);
}

namespace stm32f411::gpio {
    namespace tags {
        struct moder;
        struct otyper;
        struct ospeedr;
        struct pupdr;
        struct idr;
        struct odr;
        struct bsrr;
        struct lckr;
        struct afrl;
        struct afrh;
    }

    using MODER = register_t<tags::moder>;
    using OTYPER = register_t<tags::otyper>;
    using OSPEEDR = register_t<tags::ospeedr>;
    using PUPDR = register_t<tags::pupdr>;
    using IDR = register_t<tags::idr>;
    using ODR = register_t<tags::odr>;
    using BSRR = write_only_register_t<tags::bsrr>;
    using LCKR = register_t<tags::lckr>;
    using AFRL = register_t<tags::afrl>;
    using AFRH = register_t<tags::afrh>;

    namespace moder {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr field_t<tags::moder, Pin * 2, 2> mode {};
    }

    namespace otyper {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr field_t<tags::otyper, Pin> type {};
    }

    namespace ospeedr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr field_t<tags::ospeedr, Pin * 2, 2> speed {};
    }

    namespace pupdr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr field_t<tags::pupdr, Pin * 2, 2> pull {};
    }

    namespace idr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr field_t<tags::idr, Pin> input {};
    }

    namespace odr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr field_t<tags::odr, Pin> output {};
    }

    namespace bsrr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr field_t<tags::bsrr, Pin> set {};

        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr field_t<tags::bsrr, Pin + 16> reset {};
    }

    namespace lckr {
        template<std::uint32_t Pin>
        requires(Pin < 16)
        inline constexpr field_t<tags::lckr, Pin> lock {};

        inline constexpr field_t<tags::lckr, 16> key {};
    }

    namespace afrl {
        template<std::uint32_t Pin>
        requires(Pin < 8)
        inline constexpr field_t<tags::afrl, Pin * 4, 4> alternate_function {};
    }

    namespace afrh {
        template<std::uint32_t Pin>
        requires(Pin >= 8 && Pin < 16)
        inline constexpr field_t<tags::afrh, (Pin - 8) * 4, 4> alternate_function {};
    }

    struct registers_t
    {
        gpio::MODER   MODER;
        gpio::OTYPER  OTYPER;
        gpio::OSPEEDR OSPEEDR;
        gpio::PUPDR   PUPDR;
        gpio::IDR     IDR;
        gpio::ODR     ODR;
        gpio::BSRR    BSRR;
        gpio::LCKR    LCKR;
        gpio::AFRL    AFRL;
        gpio::AFRH    AFRH;
    };

    static_assert(offsetof(registers_t, MODER) == 0x00);
    static_assert(offsetof(registers_t, OTYPER) == 0x04);
    static_assert(offsetof(registers_t, OSPEEDR) == 0x08);
    static_assert(offsetof(registers_t, PUPDR) == 0x0C);
    static_assert(offsetof(registers_t, IDR) == 0x10);
    static_assert(offsetof(registers_t, ODR) == 0x14);
    static_assert(offsetof(registers_t, BSRR) == 0x18);
    static_assert(offsetof(registers_t, LCKR) == 0x1C);
    static_assert(offsetof(registers_t, AFRL) == 0x20);
    static_assert(offsetof(registers_t, AFRH) == 0x24);
    static_assert(sizeof(registers_t) == 0x28);

    inline constexpr std::uintptr_t gpioa_address = 0x40020000UL;
    inline constexpr std::uintptr_t gpiob_address = 0x40020400UL;
    inline constexpr std::uintptr_t gpioc_address = 0x40020800UL;
    inline constexpr std::uintptr_t gpiod_address = 0x40020C00UL;
    inline constexpr std::uintptr_t gpioe_address = 0x40021000UL;
    inline constexpr std::uintptr_t gpioh_address = 0x40021C00UL;

    inline registers_t* const GPIOA = reinterpret_cast<registers_t*>(gpioa_address);
    inline registers_t* const GPIOB = reinterpret_cast<registers_t*>(gpiob_address);
    inline registers_t* const GPIOC = reinterpret_cast<registers_t*>(gpioc_address);
    inline registers_t* const GPIOD = reinterpret_cast<registers_t*>(gpiod_address);
    inline registers_t* const GPIOE = reinterpret_cast<registers_t*>(gpioe_address);
    inline registers_t* const GPIOH = reinterpret_cast<registers_t*>(gpioh_address);

    enum class mode_t : std::uint8_t
    {
        input = 0,
        output = 1,
        alt_func = 2,
        analog = 3
    };

    enum class output_type_t : std::uint8_t
    {
        push_pull = 0,
        open_drain = 1
    };

    enum class output_speed_t : std::uint8_t
    {
        low = 0,
        medium = 1,
        fast = 2,
        high = 3
    };

    enum class pull_up_down_t : std::uint8_t
    {
        no_pull_up_down = 0,
        pull_up = 1,
        pull_down = 2
    };

    enum class alternate_function_t : std::uint8_t
    {
        AF0 = 0,
        AF1,
        AF2,
        AF3,
        AF4,
        AF5,
        AF6,
        AF7,
        AF8,
        AF9,
        AF10,
        AF11,
        AF12,
        AF13,
        AF14,
        AF15
    };

    struct config_t
    {
        mode_t               port_mode { mode_t::input };
        output_type_t        output_type { output_type_t::push_pull };
        output_speed_t       output_speed { output_speed_t::low };
        pull_up_down_t       pull_up_down { pull_up_down_t::no_pull_up_down };
        alternate_function_t alt_func { alternate_function_t::AF0 };
    };

    template<std::uint32_t Pin, std::uintptr_t Address>
    requires(Pin < 16)
    struct pin_t
    {
    private:
        static registers_t* gpio() noexcept
        {
            return reinterpret_cast<registers_t*>(Address);
        }

    public:
        [[nodiscard]] bool read_output() const noexcept
        {
            return gpio()->ODR.read(odr::output<Pin>);
        }

        [[nodiscard]] bool read() const noexcept
        {
            return gpio()->IDR.read(idr::input<Pin>);
        }

        void write(const bool value) const noexcept
        {
            value ? set() : reset();
        }

        void set() const noexcept
        {
            gpio()->BSRR.set(bsrr::set<Pin>);
        }

        void reset() const noexcept
        {
            gpio()->BSRR.set(bsrr::reset<Pin>);
        }

        void configure(const config_t& cfg) const noexcept
        {
            gpio()->OTYPER.write(otyper::type<Pin>, std::to_underlying(cfg.output_type));
            gpio()->OSPEEDR.write(ospeedr::speed<Pin>, std::to_underlying(cfg.output_speed));
            gpio()->PUPDR.write(pupdr::pull<Pin>, std::to_underlying(cfg.pull_up_down));

            if constexpr (Pin < 8)
            {
                gpio()->AFRL.write(afrl::alternate_function<Pin>, std::to_underlying(cfg.alt_func));
            }
            else
            {
                gpio()->AFRH.write(afrh::alternate_function<Pin>, std::to_underlying(cfg.alt_func));
            }

            gpio()->MODER.write(moder::mode<Pin>, std::to_underlying(cfg.port_mode));
        }
    };

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpioa_address> PA {};

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpiob_address> PB {};

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpioc_address> PC {};

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpiod_address> PD {};

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpioe_address> PE {};

    template<std::uint32_t Pin>
    inline constexpr pin_t<Pin, gpioh_address> PH {};

    inline constexpr auto PA0 = PA<0>;
    inline constexpr auto PA1 = PA<1>;
    inline constexpr auto PA2 = PA<2>;
    inline constexpr auto PA3 = PA<3>;
    inline constexpr auto PA4 = PA<4>;
    inline constexpr auto PA5 = PA<5>;
}

namespace stm32f411::adc {
    namespace tags {
        struct sr;
        struct cr1;
        struct cr2;
        struct smpr1;
        struct smpr2;
        struct jofr1;
        struct jofr2;
        struct jofr3;
        struct jofr4;
        struct htr;
        struct ltr;
        struct sqr1;
        struct sqr2;
        struct sqr3;
        struct jsqr;
        struct jdr1;
        struct jdr2;
        struct jdr3;
        struct jdr4;
        struct dr;
        struct csr;
        struct ccr;
        struct cdr;
    }

    using SR = register_t<tags::sr>;
    using CR1 = register_t<tags::cr1>;
    using CR2 = register_t<tags::cr2>;
    using SMPR1 = register_t<tags::smpr1>;
    using SMPR2 = register_t<tags::smpr2>;
    using JOFR1 = register_t<tags::jofr1>;
    using JOFR2 = register_t<tags::jofr2>;
    using JOFR3 = register_t<tags::jofr3>;
    using JOFR4 = register_t<tags::jofr4>;
    using HTR = register_t<tags::htr>;
    using LTR = register_t<tags::ltr>;
    using SQR1 = register_t<tags::sqr1>;
    using SQR2 = register_t<tags::sqr2>;
    using SQR3 = register_t<tags::sqr3>;
    using JSQR = register_t<tags::jsqr>;
    using JDR1 = register_t<tags::jdr1>;
    using JDR2 = register_t<tags::jdr2>;
    using JDR3 = register_t<tags::jdr3>;
    using JDR4 = register_t<tags::jdr4>;
    using DR = register_t<tags::dr>;
    using CSR = register_t<tags::csr>;
    using CCR = register_t<tags::ccr>;
    using CDR = register_t<tags::cdr>;

    namespace sr {
        inline constexpr field_t<tags::sr, 0> awd {};
        inline constexpr field_t<tags::sr, 1> eoc {};
        inline constexpr field_t<tags::sr, 2> jeoc {};
        inline constexpr field_t<tags::sr, 3> jstrt {};
        inline constexpr field_t<tags::sr, 4> strt {};
        inline constexpr field_t<tags::sr, 5> ovr {};
    }

    namespace cr1 {
        inline constexpr field_t<tags::cr1, 0, 5>  awdch {};
        inline constexpr field_t<tags::cr1, 5>     eocie {};
        inline constexpr field_t<tags::cr1, 6>     awdie {};
        inline constexpr field_t<tags::cr1, 7>     jeocie {};
        inline constexpr field_t<tags::cr1, 8>     scan {};
        inline constexpr field_t<tags::cr1, 9>     awdsgl {};
        inline constexpr field_t<tags::cr1, 10>    jauto {};
        inline constexpr field_t<tags::cr1, 11>    discen {};
        inline constexpr field_t<tags::cr1, 12>    jdiscen {};
        inline constexpr field_t<tags::cr1, 13, 3> discnum {};
        inline constexpr field_t<tags::cr1, 22>    jawden {};
        inline constexpr field_t<tags::cr1, 23>    awden {};
        inline constexpr field_t<tags::cr1, 24, 2> res {};
        inline constexpr field_t<tags::cr1, 26>    ovrie {};
    }

    namespace cr2 {
        inline constexpr field_t<tags::cr2, 0>     adon {};
        inline constexpr field_t<tags::cr2, 1>     cont {};
        inline constexpr field_t<tags::cr2, 8>     dma {};
        inline constexpr field_t<tags::cr2, 9>     dds {};
        inline constexpr field_t<tags::cr2, 10>    eocs {};
        inline constexpr field_t<tags::cr2, 11>    align {};
        inline constexpr field_t<tags::cr2, 16, 4> jextsel {};
        inline constexpr field_t<tags::cr2, 20, 2> jexten {};
        inline constexpr field_t<tags::cr2, 22>    jswstart {};
        inline constexpr field_t<tags::cr2, 24, 4> extsel {};
        inline constexpr field_t<tags::cr2, 28, 2> exten {};
        inline constexpr field_t<tags::cr2, 30>    swstart {};
    }

    namespace smpr1 {
        inline constexpr field_t<tags::smpr1, 0, 3>  smp10 {};
        inline constexpr field_t<tags::smpr1, 3, 3>  smp11 {};
        inline constexpr field_t<tags::smpr1, 6, 3>  smp12 {};
        inline constexpr field_t<tags::smpr1, 9, 3>  smp13 {};
        inline constexpr field_t<tags::smpr1, 12, 3> smp14 {};
        inline constexpr field_t<tags::smpr1, 15, 3> smp15 {};
        inline constexpr field_t<tags::smpr1, 18, 3> smp16 {};
        inline constexpr field_t<tags::smpr1, 21, 3> smp17 {};
        inline constexpr field_t<tags::smpr1, 24, 3> smp18 {};
    }

    namespace smpr2 {
        inline constexpr field_t<tags::smpr2, 0, 3>  smp0 {};
        inline constexpr field_t<tags::smpr2, 3, 3>  smp1 {};
        inline constexpr field_t<tags::smpr2, 6, 3>  smp2 {};
        inline constexpr field_t<tags::smpr2, 9, 3>  smp3 {};
        inline constexpr field_t<tags::smpr2, 12, 3> smp4 {};
        inline constexpr field_t<tags::smpr2, 15, 3> smp5 {};
        inline constexpr field_t<tags::smpr2, 18, 3> smp6 {};
        inline constexpr field_t<tags::smpr2, 21, 3> smp7 {};
        inline constexpr field_t<tags::smpr2, 24, 3> smp8 {};
        inline constexpr field_t<tags::smpr2, 27, 3> smp9 {};
    }

    namespace jofr1 {
        inline constexpr field_t<tags::jofr1, 0, 12> joffset1 {};
    }

    namespace jofr2 {
        inline constexpr field_t<tags::jofr2, 0, 12> joffset2 {};
    }

    namespace jofr3 {
        inline constexpr field_t<tags::jofr3, 0, 12> joffset3 {};
    }

    namespace jofr4 {
        inline constexpr field_t<tags::jofr4, 0, 12> joffset4 {};
    }

    namespace htr {
        inline constexpr field_t<tags::htr, 0, 12> ht {};
    }

    namespace ltr {
        inline constexpr field_t<tags::ltr, 0, 12> lt {};
    }

    namespace sqr1 {
        inline constexpr field_t<tags::sqr1, 0, 5>  sq13 {};
        inline constexpr field_t<tags::sqr1, 5, 5>  sq14 {};
        inline constexpr field_t<tags::sqr1, 10, 5> sq15 {};
        inline constexpr field_t<tags::sqr1, 15, 5> sq16 {};
        inline constexpr field_t<tags::sqr1, 20, 4> l {};
    }

    namespace sqr2 {
        inline constexpr field_t<tags::sqr2, 0, 5>  sq7 {};
        inline constexpr field_t<tags::sqr2, 5, 5>  sq8 {};
        inline constexpr field_t<tags::sqr2, 10, 5> sq9 {};
        inline constexpr field_t<tags::sqr2, 15, 5> sq10 {};
        inline constexpr field_t<tags::sqr2, 20, 5> sq11 {};
        inline constexpr field_t<tags::sqr2, 25, 5> sq12 {};
    }

    namespace sqr3 {
        inline constexpr field_t<tags::sqr3, 0, 5>  sq1 {};
        inline constexpr field_t<tags::sqr3, 5, 5>  sq2 {};
        inline constexpr field_t<tags::sqr3, 10, 5> sq3 {};
        inline constexpr field_t<tags::sqr3, 15, 5> sq4 {};
        inline constexpr field_t<tags::sqr3, 20, 5> sq5 {};
        inline constexpr field_t<tags::sqr3, 25, 5> sq6 {};
    }

    namespace jsqr {
        inline constexpr field_t<tags::jsqr, 0, 5>  jsq1 {};
        inline constexpr field_t<tags::jsqr, 5, 5>  jsq2 {};
        inline constexpr field_t<tags::jsqr, 10, 5> jsq3 {};
        inline constexpr field_t<tags::jsqr, 15, 5> jsq4 {};
        inline constexpr field_t<tags::jsqr, 20, 2> jl {};
    }

    namespace jdr1 {
        inline constexpr field_t<tags::jdr1, 0, 16> jdata {};
    }

    namespace jdr2 {
        inline constexpr field_t<tags::jdr2, 0, 16> jdata {};
    }

    namespace jdr3 {
        inline constexpr field_t<tags::jdr3, 0, 16> jdata {};
    }

    namespace jdr4 {
        inline constexpr field_t<tags::jdr4, 0, 16> jdata {};
    }

    namespace dr {
        inline constexpr field_t<tags::dr, 0, 16>  data {};
        inline constexpr field_t<tags::dr, 16, 16> adc2data {};
    }

    namespace csr {
        inline constexpr field_t<tags::csr, 0> awd1 {};
        inline constexpr field_t<tags::csr, 1> eoc1 {};
        inline constexpr field_t<tags::csr, 2> jeoc1 {};
        inline constexpr field_t<tags::csr, 3> jstrt1 {};
        inline constexpr field_t<tags::csr, 4> strt1 {};
        inline constexpr field_t<tags::csr, 5> ovr1 {};
    }

    namespace ccr {
        inline constexpr field_t<tags::ccr, 0, 5>  multi {};
        inline constexpr field_t<tags::ccr, 8, 4>  delay {};
        inline constexpr field_t<tags::ccr, 13>    dds {};
        inline constexpr field_t<tags::ccr, 14, 2> dma {};
        inline constexpr field_t<tags::ccr, 16, 2> adcpre {};
        inline constexpr field_t<tags::ccr, 22>    vbate {};
        inline constexpr field_t<tags::ccr, 23>    tsvrefe {};
    }

    namespace cdr {
        inline constexpr field_t<tags::cdr, 0, 16>  data1 {};
        inline constexpr field_t<tags::cdr, 16, 16> data2 {};
    }
    struct registers_t
    {
        adc::SR    SR;
        adc::CR1   CR1;
        adc::CR2   CR2;
        adc::SMPR1 SMPR1;
        adc::SMPR2 SMPR2;
        adc::JOFR1 JOFR1;
        adc::JOFR2 JOFR2;
        adc::JOFR3 JOFR3;
        adc::JOFR4 JOFR4;
        adc::HTR   HTR;
        adc::LTR   LTR;
        adc::SQR1  SQR1;
        adc::SQR2  SQR2;
        adc::SQR3  SQR3;
        adc::JSQR  JSQR;
        adc::JDR1  JDR1;
        adc::JDR2  JDR2;
        adc::JDR3  JDR3;
        adc::JDR4  JDR4;
        adc::DR    DR;
    };

    struct common_registers_t
    {
        adc::CSR CSR;
        adc::CCR CCR;
        adc::CDR CDR;
    };

    static_assert(offsetof(registers_t, SR) == 0x00);
    static_assert(offsetof(registers_t, DR) == 0x4C);
    static_assert(sizeof(registers_t) == 0x50);
    static_assert(offsetof(common_registers_t, CSR) == 0x00);
    static_assert(offsetof(common_registers_t, CDR) == 0x08);
    static_assert(sizeof(common_registers_t) == 0x0C);

    inline registers_t* const        ADC1 = reinterpret_cast<registers_t*>(0x40012000UL);
    inline common_registers_t* const COMMON = reinterpret_cast<common_registers_t*>(0x40012300UL);
}

namespace stm32f411::crc {
    namespace tags {
        struct dr;
        struct idr;
        struct cr;
    }

    using DR = register_t<tags::dr>;
    using IDR = byte_register_t<tags::idr>;
    using CR = register_t<tags::cr>;

    namespace dr {
        inline constexpr field_t<tags::dr, 0, 32> dr {};
    }

    namespace idr {
        inline constexpr field_t<tags::idr, 0, 8> idr {};
    }

    namespace cr {
        inline constexpr field_t<tags::cr, 0> reset {};
    }
    struct registers_t
    {
        crc::DR       DR;
        crc::IDR      IDR;
        std::uint8_t  reserved_x05;
        std::uint16_t reserved_x06;
        crc::CR       CR;
    };

    static_assert(offsetof(registers_t, DR) == 0x00);
    static_assert(offsetof(registers_t, IDR) == 0x04);
    static_assert(offsetof(registers_t, CR) == 0x08);
    static_assert(sizeof(registers_t) == 0x0C);

    inline registers_t* const CRC = reinterpret_cast<registers_t*>(0x40023000UL);
}

namespace stm32f411::dbgmcu {
    namespace tags {
        struct idcode;
        struct cr;
        struct apb1fz;
        struct apb2fz;
    }

    using IDCODE = read_only_register_t<tags::idcode>;
    using CR = register_t<tags::cr>;
    using APB1FZ = register_t<tags::apb1fz>;
    using APB2FZ = register_t<tags::apb2fz>;

    namespace idcode {
        inline constexpr field_t<tags::idcode, 0, 12>  dev_id {};
        inline constexpr field_t<tags::idcode, 16, 16> rev_id {};
    }

    namespace cr {
        inline constexpr field_t<tags::cr, 0>    dbg_sleep {};
        inline constexpr field_t<tags::cr, 1>    dbg_stop {};
        inline constexpr field_t<tags::cr, 2>    dbg_standby {};
        inline constexpr field_t<tags::cr, 5>    trace_ioen {};
        inline constexpr field_t<tags::cr, 6, 2> trace_mode {};
    }

    namespace apb1fz {
        inline constexpr field_t<tags::apb1fz, 0>  dbg_tim2_stop {};
        inline constexpr field_t<tags::apb1fz, 1>  dbg_tim3_stop {};
        inline constexpr field_t<tags::apb1fz, 2>  dbg_tim4_stop {};
        inline constexpr field_t<tags::apb1fz, 3>  dbg_tim5_stop {};
        inline constexpr field_t<tags::apb1fz, 10> dbg_rtc_stop {};
        inline constexpr field_t<tags::apb1fz, 11> dbg_wwdg_stop {};
        inline constexpr field_t<tags::apb1fz, 12> dbg_iwdg_stop {};
        inline constexpr field_t<tags::apb1fz, 21> dbg_i2c1_smbus_timeout {};
        inline constexpr field_t<tags::apb1fz, 22> dbg_i2c2_smbus_timeout {};
        inline constexpr field_t<tags::apb1fz, 23> dbg_i2c3_smbus_timeout {};
    }

    namespace apb2fz {
        inline constexpr field_t<tags::apb2fz, 0>  dbg_tim1_stop {};
        inline constexpr field_t<tags::apb2fz, 16> dbg_tim9_stop {};
        inline constexpr field_t<tags::apb2fz, 17> dbg_tim10_stop {};
        inline constexpr field_t<tags::apb2fz, 18> dbg_tim11_stop {};
    }
    struct registers_t
    {
        dbgmcu::IDCODE IDCODE;
        dbgmcu::CR     CR;
        dbgmcu::APB1FZ APB1FZ;
        dbgmcu::APB2FZ APB2FZ;
    };

    static_assert(offsetof(registers_t, IDCODE) == 0x00);
    static_assert(offsetof(registers_t, APB2FZ) == 0x0C);
    static_assert(sizeof(registers_t) == 0x10);

    inline registers_t* const DBGMCU = reinterpret_cast<registers_t*>(0xE0042000UL);
}

namespace stm32f411::dma {
    namespace tags {
        struct lisr;
        struct hisr;
        struct lifcr;
        struct hifcr;
        struct cr;
        struct ndtr;
        struct par;
        struct m0ar;
        struct m1ar;
        struct fcr;
    }

    using LISR = read_only_register_t<tags::lisr>;
    using HISR = read_only_register_t<tags::hisr>;
    using LIFCR = register_t<tags::lifcr>;
    using HIFCR = register_t<tags::hifcr>;
    using CR = register_t<tags::cr>;
    using NDTR = register_t<tags::ndtr>;
    using PAR = register_t<tags::par>;
    using M0AR = register_t<tags::m0ar>;
    using M1AR = register_t<tags::m1ar>;
    using FCR = register_t<tags::fcr>;

    namespace lisr {
        inline constexpr field_t<tags::lisr, 0>  feif0 {};
        inline constexpr field_t<tags::lisr, 2>  dmeif0 {};
        inline constexpr field_t<tags::lisr, 3>  teif0 {};
        inline constexpr field_t<tags::lisr, 4>  htif0 {};
        inline constexpr field_t<tags::lisr, 5>  tcif0 {};
        inline constexpr field_t<tags::lisr, 6>  feif1 {};
        inline constexpr field_t<tags::lisr, 8>  dmeif1 {};
        inline constexpr field_t<tags::lisr, 9>  teif1 {};
        inline constexpr field_t<tags::lisr, 10> htif1 {};
        inline constexpr field_t<tags::lisr, 11> tcif1 {};
        inline constexpr field_t<tags::lisr, 16> feif2 {};
        inline constexpr field_t<tags::lisr, 18> dmeif2 {};
        inline constexpr field_t<tags::lisr, 19> teif2 {};
        inline constexpr field_t<tags::lisr, 20> htif2 {};
        inline constexpr field_t<tags::lisr, 21> tcif2 {};
        inline constexpr field_t<tags::lisr, 22> feif3 {};
        inline constexpr field_t<tags::lisr, 24> dmeif3 {};
        inline constexpr field_t<tags::lisr, 25> teif3 {};
        inline constexpr field_t<tags::lisr, 26> htif3 {};
        inline constexpr field_t<tags::lisr, 27> tcif3 {};
    }

    namespace hisr {
        inline constexpr field_t<tags::hisr, 0>  feif4 {};
        inline constexpr field_t<tags::hisr, 2>  dmeif4 {};
        inline constexpr field_t<tags::hisr, 3>  teif4 {};
        inline constexpr field_t<tags::hisr, 4>  htif4 {};
        inline constexpr field_t<tags::hisr, 5>  tcif4 {};
        inline constexpr field_t<tags::hisr, 6>  feif5 {};
        inline constexpr field_t<tags::hisr, 8>  dmeif5 {};
        inline constexpr field_t<tags::hisr, 9>  teif5 {};
        inline constexpr field_t<tags::hisr, 10> htif5 {};
        inline constexpr field_t<tags::hisr, 11> tcif5 {};
        inline constexpr field_t<tags::hisr, 16> feif6 {};
        inline constexpr field_t<tags::hisr, 18> dmeif6 {};
        inline constexpr field_t<tags::hisr, 19> teif6 {};
        inline constexpr field_t<tags::hisr, 20> htif6 {};
        inline constexpr field_t<tags::hisr, 21> tcif6 {};
        inline constexpr field_t<tags::hisr, 22> feif7 {};
        inline constexpr field_t<tags::hisr, 24> dmeif7 {};
        inline constexpr field_t<tags::hisr, 25> teif7 {};
        inline constexpr field_t<tags::hisr, 26> htif7 {};
        inline constexpr field_t<tags::hisr, 27> tcif7 {};
    }

    namespace lifcr {
        inline constexpr field_t<tags::lifcr, 0>  cfeif0 {};
        inline constexpr field_t<tags::lifcr, 2>  cdmeif0 {};
        inline constexpr field_t<tags::lifcr, 3>  cteif0 {};
        inline constexpr field_t<tags::lifcr, 4>  chtif0 {};
        inline constexpr field_t<tags::lifcr, 5>  ctcif0 {};
        inline constexpr field_t<tags::lifcr, 6>  cfeif1 {};
        inline constexpr field_t<tags::lifcr, 8>  cdmeif1 {};
        inline constexpr field_t<tags::lifcr, 9>  cteif1 {};
        inline constexpr field_t<tags::lifcr, 10> chtif1 {};
        inline constexpr field_t<tags::lifcr, 11> ctcif1 {};
        inline constexpr field_t<tags::lifcr, 16> cfeif2 {};
        inline constexpr field_t<tags::lifcr, 18> cdmeif2 {};
        inline constexpr field_t<tags::lifcr, 19> cteif2 {};
        inline constexpr field_t<tags::lifcr, 20> chtif2 {};
        inline constexpr field_t<tags::lifcr, 21> ctcif2 {};
        inline constexpr field_t<tags::lifcr, 22> cfeif3 {};
        inline constexpr field_t<tags::lifcr, 24> cdmeif3 {};
        inline constexpr field_t<tags::lifcr, 25> cteif3 {};
        inline constexpr field_t<tags::lifcr, 26> chtif3 {};
        inline constexpr field_t<tags::lifcr, 27> ctcif3 {};
    }

    namespace hifcr {
        inline constexpr field_t<tags::hifcr, 0>  cfeif4 {};
        inline constexpr field_t<tags::hifcr, 2>  cdmeif4 {};
        inline constexpr field_t<tags::hifcr, 3>  cteif4 {};
        inline constexpr field_t<tags::hifcr, 4>  chtif4 {};
        inline constexpr field_t<tags::hifcr, 5>  ctcif4 {};
        inline constexpr field_t<tags::hifcr, 6>  cfeif5 {};
        inline constexpr field_t<tags::hifcr, 8>  cdmeif5 {};
        inline constexpr field_t<tags::hifcr, 9>  cteif5 {};
        inline constexpr field_t<tags::hifcr, 10> chtif5 {};
        inline constexpr field_t<tags::hifcr, 11> ctcif5 {};
        inline constexpr field_t<tags::hifcr, 16> cfeif6 {};
        inline constexpr field_t<tags::hifcr, 18> cdmeif6 {};
        inline constexpr field_t<tags::hifcr, 19> cteif6 {};
        inline constexpr field_t<tags::hifcr, 20> chtif6 {};
        inline constexpr field_t<tags::hifcr, 21> ctcif6 {};
        inline constexpr field_t<tags::hifcr, 22> cfeif7 {};
        inline constexpr field_t<tags::hifcr, 24> cdmeif7 {};
        inline constexpr field_t<tags::hifcr, 25> cteif7 {};
        inline constexpr field_t<tags::hifcr, 26> chtif7 {};
        inline constexpr field_t<tags::hifcr, 27> ctcif7 {};
    }

    namespace cr {
        inline constexpr field_t<tags::cr, 0>     en {};
        inline constexpr field_t<tags::cr, 1>     dmeie {};
        inline constexpr field_t<tags::cr, 2>     teie {};
        inline constexpr field_t<tags::cr, 3>     htie {};
        inline constexpr field_t<tags::cr, 4>     tcie {};
        inline constexpr field_t<tags::cr, 5>     pfctrl {};
        inline constexpr field_t<tags::cr, 6, 2>  dir {};
        inline constexpr field_t<tags::cr, 8>     circ {};
        inline constexpr field_t<tags::cr, 9>     pinc {};
        inline constexpr field_t<tags::cr, 10>    minc {};
        inline constexpr field_t<tags::cr, 11, 2> psize {};
        inline constexpr field_t<tags::cr, 13, 2> msize {};
        inline constexpr field_t<tags::cr, 15>    pincos {};
        inline constexpr field_t<tags::cr, 16, 2> pl {};
        inline constexpr field_t<tags::cr, 18>    dbm {};
        inline constexpr field_t<tags::cr, 19>    ct {};
        inline constexpr field_t<tags::cr, 20>    ack {};
        inline constexpr field_t<tags::cr, 21, 2> pburst {};
        inline constexpr field_t<tags::cr, 23, 2> mburst {};
        inline constexpr field_t<tags::cr, 25, 3> chsel {};
    }

    namespace ndtr {
        inline constexpr field_t<tags::ndtr, 0, 16> ndt {};
    }

    namespace par {
        inline constexpr field_t<tags::par, 0, 32> pa {};
    }

    namespace m0ar {
        inline constexpr field_t<tags::m0ar, 0, 32> m0a {};
    }

    namespace m1ar {
        inline constexpr field_t<tags::m1ar, 0, 32> m1a {};
    }

    namespace fcr {
        inline constexpr field_t<tags::fcr, 0, 2> fth {};
        inline constexpr field_t<tags::fcr, 2>    dmdis {};
        inline constexpr field_t<tags::fcr, 3, 3> fs {};
        inline constexpr field_t<tags::fcr, 7>    feie {};
    }
    struct controller_registers_t
    {
        dma::LISR  LISR;
        dma::HISR  HISR;
        dma::LIFCR LIFCR;
        dma::HIFCR HIFCR;
    };

    struct stream_registers_t
    {
        dma::CR   CR;
        dma::NDTR NDTR;
        dma::PAR  PAR;
        dma::M0AR M0AR;
        dma::M1AR M1AR;
        dma::FCR  FCR;
    };

    static_assert(offsetof(controller_registers_t, LISR) == 0x00);
    static_assert(offsetof(controller_registers_t, HIFCR) == 0x0C);
    static_assert(sizeof(controller_registers_t) == 0x10);
    static_assert(offsetof(stream_registers_t, CR) == 0x00);
    static_assert(offsetof(stream_registers_t, FCR) == 0x14);
    static_assert(sizeof(stream_registers_t) == 0x18);

    inline controller_registers_t* const DMA1 = reinterpret_cast<controller_registers_t*>(0x40026000UL);
    inline controller_registers_t* const DMA2 = reinterpret_cast<controller_registers_t*>(0x40026400UL);

    inline stream_registers_t* const DMA1_STREAM0 = reinterpret_cast<stream_registers_t*>(0x40026010UL);
    inline stream_registers_t* const DMA1_STREAM1 = reinterpret_cast<stream_registers_t*>(0x40026028UL);
    inline stream_registers_t* const DMA1_STREAM2 = reinterpret_cast<stream_registers_t*>(0x40026040UL);
    inline stream_registers_t* const DMA1_STREAM3 = reinterpret_cast<stream_registers_t*>(0x40026058UL);
    inline stream_registers_t* const DMA1_STREAM4 = reinterpret_cast<stream_registers_t*>(0x40026070UL);
    inline stream_registers_t* const DMA1_STREAM5 = reinterpret_cast<stream_registers_t*>(0x40026088UL);
    inline stream_registers_t* const DMA1_STREAM6 = reinterpret_cast<stream_registers_t*>(0x400260A0UL);
    inline stream_registers_t* const DMA1_STREAM7 = reinterpret_cast<stream_registers_t*>(0x400260B8UL);
    inline stream_registers_t* const DMA2_STREAM0 = reinterpret_cast<stream_registers_t*>(0x40026410UL);
    inline stream_registers_t* const DMA2_STREAM1 = reinterpret_cast<stream_registers_t*>(0x40026428UL);
    inline stream_registers_t* const DMA2_STREAM2 = reinterpret_cast<stream_registers_t*>(0x40026440UL);
    inline stream_registers_t* const DMA2_STREAM3 = reinterpret_cast<stream_registers_t*>(0x40026458UL);
    inline stream_registers_t* const DMA2_STREAM4 = reinterpret_cast<stream_registers_t*>(0x40026470UL);
    inline stream_registers_t* const DMA2_STREAM5 = reinterpret_cast<stream_registers_t*>(0x40026488UL);
    inline stream_registers_t* const DMA2_STREAM6 = reinterpret_cast<stream_registers_t*>(0x400264A0UL);
    inline stream_registers_t* const DMA2_STREAM7 = reinterpret_cast<stream_registers_t*>(0x400264B8UL);
}

namespace stm32f411::exti {
    namespace tags {
        struct imr;
        struct emr;
        struct rtsr;
        struct ftsr;
        struct swier;
        struct pr;
    }

    using IMR = register_t<tags::imr>;
    using EMR = register_t<tags::emr>;
    using RTSR = register_t<tags::rtsr>;
    using FTSR = register_t<tags::ftsr>;
    using SWIER = register_t<tags::swier>;
    using PR = register_t<tags::pr>;

    namespace imr {
        inline constexpr field_t<tags::imr, 0, 23> im {};
        inline constexpr field_t<tags::imr, 0>     mr0 {};
        inline constexpr field_t<tags::imr, 1>     mr1 {};
        inline constexpr field_t<tags::imr, 2>     mr2 {};
        inline constexpr field_t<tags::imr, 3>     mr3 {};
        inline constexpr field_t<tags::imr, 4>     mr4 {};
        inline constexpr field_t<tags::imr, 5>     mr5 {};
        inline constexpr field_t<tags::imr, 6>     mr6 {};
        inline constexpr field_t<tags::imr, 7>     mr7 {};
        inline constexpr field_t<tags::imr, 8>     mr8 {};
        inline constexpr field_t<tags::imr, 9>     mr9 {};
        inline constexpr field_t<tags::imr, 10>    mr10 {};
        inline constexpr field_t<tags::imr, 11>    mr11 {};
        inline constexpr field_t<tags::imr, 12>    mr12 {};
        inline constexpr field_t<tags::imr, 13>    mr13 {};
        inline constexpr field_t<tags::imr, 14>    mr14 {};
        inline constexpr field_t<tags::imr, 15>    mr15 {};
        inline constexpr field_t<tags::imr, 16>    mr16 {};
        inline constexpr field_t<tags::imr, 17>    mr17 {};
        inline constexpr field_t<tags::imr, 18>    mr18 {};
        inline constexpr field_t<tags::imr, 19>    mr19 {};
        inline constexpr field_t<tags::imr, 20>    mr20 {};
        inline constexpr field_t<tags::imr, 21>    mr21 {};
        inline constexpr field_t<tags::imr, 22>    mr22 {};
    }

    namespace emr {
        inline constexpr field_t<tags::emr, 0>  mr0 {};
        inline constexpr field_t<tags::emr, 1>  mr1 {};
        inline constexpr field_t<tags::emr, 2>  mr2 {};
        inline constexpr field_t<tags::emr, 3>  mr3 {};
        inline constexpr field_t<tags::emr, 4>  mr4 {};
        inline constexpr field_t<tags::emr, 5>  mr5 {};
        inline constexpr field_t<tags::emr, 6>  mr6 {};
        inline constexpr field_t<tags::emr, 7>  mr7 {};
        inline constexpr field_t<tags::emr, 8>  mr8 {};
        inline constexpr field_t<tags::emr, 9>  mr9 {};
        inline constexpr field_t<tags::emr, 10> mr10 {};
        inline constexpr field_t<tags::emr, 11> mr11 {};
        inline constexpr field_t<tags::emr, 12> mr12 {};
        inline constexpr field_t<tags::emr, 13> mr13 {};
        inline constexpr field_t<tags::emr, 14> mr14 {};
        inline constexpr field_t<tags::emr, 15> mr15 {};
        inline constexpr field_t<tags::emr, 16> mr16 {};
        inline constexpr field_t<tags::emr, 17> mr17 {};
        inline constexpr field_t<tags::emr, 18> mr18 {};
        inline constexpr field_t<tags::emr, 19> mr19 {};
        inline constexpr field_t<tags::emr, 20> mr20 {};
        inline constexpr field_t<tags::emr, 21> mr21 {};
        inline constexpr field_t<tags::emr, 22> mr22 {};
    }

    namespace rtsr {
        inline constexpr field_t<tags::rtsr, 0>  tr0 {};
        inline constexpr field_t<tags::rtsr, 1>  tr1 {};
        inline constexpr field_t<tags::rtsr, 2>  tr2 {};
        inline constexpr field_t<tags::rtsr, 3>  tr3 {};
        inline constexpr field_t<tags::rtsr, 4>  tr4 {};
        inline constexpr field_t<tags::rtsr, 5>  tr5 {};
        inline constexpr field_t<tags::rtsr, 6>  tr6 {};
        inline constexpr field_t<tags::rtsr, 7>  tr7 {};
        inline constexpr field_t<tags::rtsr, 8>  tr8 {};
        inline constexpr field_t<tags::rtsr, 9>  tr9 {};
        inline constexpr field_t<tags::rtsr, 10> tr10 {};
        inline constexpr field_t<tags::rtsr, 11> tr11 {};
        inline constexpr field_t<tags::rtsr, 12> tr12 {};
        inline constexpr field_t<tags::rtsr, 13> tr13 {};
        inline constexpr field_t<tags::rtsr, 14> tr14 {};
        inline constexpr field_t<tags::rtsr, 15> tr15 {};
        inline constexpr field_t<tags::rtsr, 16> tr16 {};
        inline constexpr field_t<tags::rtsr, 17> tr17 {};
        inline constexpr field_t<tags::rtsr, 18> tr18 {};
        inline constexpr field_t<tags::rtsr, 19> tr19 {};
        inline constexpr field_t<tags::rtsr, 20> tr20 {};
        inline constexpr field_t<tags::rtsr, 21> tr21 {};
        inline constexpr field_t<tags::rtsr, 22> tr22 {};
    }

    namespace ftsr {
        inline constexpr field_t<tags::ftsr, 0>  tr0 {};
        inline constexpr field_t<tags::ftsr, 1>  tr1 {};
        inline constexpr field_t<tags::ftsr, 2>  tr2 {};
        inline constexpr field_t<tags::ftsr, 3>  tr3 {};
        inline constexpr field_t<tags::ftsr, 4>  tr4 {};
        inline constexpr field_t<tags::ftsr, 5>  tr5 {};
        inline constexpr field_t<tags::ftsr, 6>  tr6 {};
        inline constexpr field_t<tags::ftsr, 7>  tr7 {};
        inline constexpr field_t<tags::ftsr, 8>  tr8 {};
        inline constexpr field_t<tags::ftsr, 9>  tr9 {};
        inline constexpr field_t<tags::ftsr, 10> tr10 {};
        inline constexpr field_t<tags::ftsr, 11> tr11 {};
        inline constexpr field_t<tags::ftsr, 12> tr12 {};
        inline constexpr field_t<tags::ftsr, 13> tr13 {};
        inline constexpr field_t<tags::ftsr, 14> tr14 {};
        inline constexpr field_t<tags::ftsr, 15> tr15 {};
        inline constexpr field_t<tags::ftsr, 16> tr16 {};
        inline constexpr field_t<tags::ftsr, 17> tr17 {};
        inline constexpr field_t<tags::ftsr, 18> tr18 {};
        inline constexpr field_t<tags::ftsr, 19> tr19 {};
        inline constexpr field_t<tags::ftsr, 20> tr20 {};
        inline constexpr field_t<tags::ftsr, 21> tr21 {};
        inline constexpr field_t<tags::ftsr, 22> tr22 {};
    }

    namespace swier {
        inline constexpr field_t<tags::swier, 0>  swier0 {};
        inline constexpr field_t<tags::swier, 1>  swier1 {};
        inline constexpr field_t<tags::swier, 2>  swier2 {};
        inline constexpr field_t<tags::swier, 3>  swier3 {};
        inline constexpr field_t<tags::swier, 4>  swier4 {};
        inline constexpr field_t<tags::swier, 5>  swier5 {};
        inline constexpr field_t<tags::swier, 6>  swier6 {};
        inline constexpr field_t<tags::swier, 7>  swier7 {};
        inline constexpr field_t<tags::swier, 8>  swier8 {};
        inline constexpr field_t<tags::swier, 9>  swier9 {};
        inline constexpr field_t<tags::swier, 10> swier10 {};
        inline constexpr field_t<tags::swier, 11> swier11 {};
        inline constexpr field_t<tags::swier, 12> swier12 {};
        inline constexpr field_t<tags::swier, 13> swier13 {};
        inline constexpr field_t<tags::swier, 14> swier14 {};
        inline constexpr field_t<tags::swier, 15> swier15 {};
        inline constexpr field_t<tags::swier, 16> swier16 {};
        inline constexpr field_t<tags::swier, 17> swier17 {};
        inline constexpr field_t<tags::swier, 18> swier18 {};
        inline constexpr field_t<tags::swier, 19> swier19 {};
        inline constexpr field_t<tags::swier, 20> swier20 {};
        inline constexpr field_t<tags::swier, 21> swier21 {};
        inline constexpr field_t<tags::swier, 22> swier22 {};
    }

    namespace pr {
        inline constexpr field_t<tags::pr, 0>  pr0 {};
        inline constexpr field_t<tags::pr, 1>  pr1 {};
        inline constexpr field_t<tags::pr, 2>  pr2 {};
        inline constexpr field_t<tags::pr, 3>  pr3 {};
        inline constexpr field_t<tags::pr, 4>  pr4 {};
        inline constexpr field_t<tags::pr, 5>  pr5 {};
        inline constexpr field_t<tags::pr, 6>  pr6 {};
        inline constexpr field_t<tags::pr, 7>  pr7 {};
        inline constexpr field_t<tags::pr, 8>  pr8 {};
        inline constexpr field_t<tags::pr, 9>  pr9 {};
        inline constexpr field_t<tags::pr, 10> pr10 {};
        inline constexpr field_t<tags::pr, 11> pr11 {};
        inline constexpr field_t<tags::pr, 12> pr12 {};
        inline constexpr field_t<tags::pr, 13> pr13 {};
        inline constexpr field_t<tags::pr, 14> pr14 {};
        inline constexpr field_t<tags::pr, 15> pr15 {};
        inline constexpr field_t<tags::pr, 16> pr16 {};
        inline constexpr field_t<tags::pr, 17> pr17 {};
        inline constexpr field_t<tags::pr, 18> pr18 {};
        inline constexpr field_t<tags::pr, 19> pr19 {};
        inline constexpr field_t<tags::pr, 20> pr20 {};
        inline constexpr field_t<tags::pr, 21> pr21 {};
        inline constexpr field_t<tags::pr, 22> pr22 {};
    }
    struct registers_t
    {
        exti::IMR   IMR;
        exti::EMR   EMR;
        exti::RTSR  RTSR;
        exti::FTSR  FTSR;
        exti::SWIER SWIER;
        exti::PR    PR;
    };

    static_assert(offsetof(registers_t, IMR) == 0x00);
    static_assert(offsetof(registers_t, PR) == 0x14);
    static_assert(sizeof(registers_t) == 0x18);

    inline registers_t* const EXTI = reinterpret_cast<registers_t*>(0x40013C00UL);
}

namespace stm32f411::flash {
    namespace tags {
        struct acr;
        struct keyr;
        struct optkeyr;
        struct sr;
        struct cr;
        struct optcr;
        struct optcr1;
    }

    using ACR = register_t<tags::acr>;
    using KEYR = register_t<tags::keyr>;
    using OPTKEYR = register_t<tags::optkeyr>;
    using SR = register_t<tags::sr>;
    using CR = register_t<tags::cr>;
    using OPTCR = register_t<tags::optcr>;
    using OPTCR1 = register_t<tags::optcr1>;

    namespace acr {
        inline constexpr field_t<tags::acr, 0, 3> latency {};
        inline constexpr field_t<tags::acr, 8>    prften {};
        inline constexpr field_t<tags::acr, 9>    icen {};
        inline constexpr field_t<tags::acr, 10>   dcen {};
        inline constexpr field_t<tags::acr, 11>   icrst {};
        inline constexpr field_t<tags::acr, 12>   dcrst {};
    }

    namespace keyr {
        inline constexpr field_t<tags::keyr, 0, 32> key {};
    }

    namespace optkeyr {
        inline constexpr field_t<tags::optkeyr, 0, 32> key {};
    }

    namespace sr {
        inline constexpr field_t<tags::sr, 0>  eop {};
        inline constexpr field_t<tags::sr, 1>  operr {};
        inline constexpr field_t<tags::sr, 4>  wrperr {};
        inline constexpr field_t<tags::sr, 5>  pgaerr {};
        inline constexpr field_t<tags::sr, 6>  pgperr {};
        inline constexpr field_t<tags::sr, 7>  pgserr {};
        inline constexpr field_t<tags::sr, 8>  rderr {};
        inline constexpr field_t<tags::sr, 16> bsy {};
    }

    namespace cr {
        inline constexpr field_t<tags::cr, 0>    pg {};
        inline constexpr field_t<tags::cr, 1>    ser {};
        inline constexpr field_t<tags::cr, 2>    mer {};
        inline constexpr field_t<tags::cr, 3, 5> snb {};
        inline constexpr field_t<tags::cr, 8, 2> psize {};
        inline constexpr field_t<tags::cr, 16>   strt {};
        inline constexpr field_t<tags::cr, 24>   eopie {};
        inline constexpr field_t<tags::cr, 25>   errie {};
        inline constexpr field_t<tags::cr, 31>   lock {};
    }

    namespace optcr {
        inline constexpr field_t<tags::optcr, 0>      optlock {};
        inline constexpr field_t<tags::optcr, 1>      optstrt {};
        inline constexpr field_t<tags::optcr, 2, 2>   bor_lev {};
        inline constexpr field_t<tags::optcr, 5>      wdg_sw {};
        inline constexpr field_t<tags::optcr, 6>      nrst_stop {};
        inline constexpr field_t<tags::optcr, 7>      nrst_stdby {};
        inline constexpr field_t<tags::optcr, 8, 8>   rdp {};
        inline constexpr field_t<tags::optcr, 16, 12> nwrp {};
    }

    namespace optcr1 {
        inline constexpr field_t<tags::optcr1, 16, 12> nwrp {};
    }
    struct registers_t
    {
        flash::ACR     ACR;
        flash::KEYR    KEYR;
        flash::OPTKEYR OPTKEYR;
        flash::SR      SR;
        flash::CR      CR;
        flash::OPTCR   OPTCR;
        flash::OPTCR1  OPTCR1;
    };

    static_assert(offsetof(registers_t, ACR) == 0x00);
    static_assert(offsetof(registers_t, OPTCR1) == 0x18);
    static_assert(sizeof(registers_t) == 0x1C);

    inline registers_t* const FLASH = reinterpret_cast<registers_t*>(0x40023C00UL);
}

namespace stm32f411::syscfg {
    namespace tags {
        struct memrmp;
        struct pmc;
        struct exticr;
        struct cmpcr;
    }

    using MEMRMP = register_t<tags::memrmp>;
    using PMC = register_t<tags::pmc>;
    using EXTICR = register_t<tags::exticr>;
    using CMPCR = register_t<tags::cmpcr>;

    namespace memrmp {
        inline constexpr field_t<tags::memrmp, 0, 2> mem_mode {};
    }

    namespace pmc {
        inline constexpr field_t<tags::pmc, 16> adc1dc2 {};
    }

    namespace exticr {
        template<std::uint32_t Line>
        requires(Line < 16)
        inline constexpr field_t<tags::exticr, (Line % 4) * 4, 4> exti {};
    }

    namespace cmpcr {
        inline constexpr field_t<tags::cmpcr, 0> cmp_pd {};
        inline constexpr field_t<tags::cmpcr, 8> ready {};
    }
    struct registers_t
    {
        syscfg::MEMRMP MEMRMP;
        syscfg::PMC    PMC;
        syscfg::EXTICR EXTICR[4];
        std::uint32_t  reserved_x18_x1C[2];
        syscfg::CMPCR  CMPCR;
    };

    static_assert(offsetof(registers_t, MEMRMP) == 0x00);
    static_assert(offsetof(registers_t, EXTICR) == 0x08);
    static_assert(offsetof(registers_t, CMPCR) == 0x20);
    static_assert(sizeof(registers_t) == 0x24);

    inline registers_t* const SYSCFG = reinterpret_cast<registers_t*>(0x40013800UL);
}

namespace stm32f411::i2c {
    namespace tags {
        struct cr1;
        struct cr2;
        struct oar1;
        struct oar2;
        struct dr;
        struct sr1;
        struct sr2;
        struct ccr;
        struct trise;
        struct fltr;
    }

    using CR1 = register_t<tags::cr1>;
    using CR2 = register_t<tags::cr2>;
    using OAR1 = register_t<tags::oar1>;
    using OAR2 = register_t<tags::oar2>;
    using DR = register_t<tags::dr>;
    using SR1 = register_t<tags::sr1>;
    using SR2 = register_t<tags::sr2>;
    using CCR = register_t<tags::ccr>;
    using TRISE = register_t<tags::trise>;
    using FLTR = register_t<tags::fltr>;

    namespace cr1 {
        inline constexpr field_t<tags::cr1, 0>  pe {};
        inline constexpr field_t<tags::cr1, 1>  smbus {};
        inline constexpr field_t<tags::cr1, 3>  smbtype {};
        inline constexpr field_t<tags::cr1, 4>  enarp {};
        inline constexpr field_t<tags::cr1, 5>  enpec {};
        inline constexpr field_t<tags::cr1, 6>  engc {};
        inline constexpr field_t<tags::cr1, 7>  nostretch {};
        inline constexpr field_t<tags::cr1, 8>  start {};
        inline constexpr field_t<tags::cr1, 9>  stop {};
        inline constexpr field_t<tags::cr1, 10> ack {};
        inline constexpr field_t<tags::cr1, 11> pos {};
        inline constexpr field_t<tags::cr1, 12> pec {};
        inline constexpr field_t<tags::cr1, 13> alert {};
        inline constexpr field_t<tags::cr1, 15> swrst {};
    }

    namespace cr2 {
        inline constexpr field_t<tags::cr2, 0, 6> freq {};
        inline constexpr field_t<tags::cr2, 8>    iterren {};
        inline constexpr field_t<tags::cr2, 9>    itevten {};
        inline constexpr field_t<tags::cr2, 10>   itbufen {};
        inline constexpr field_t<tags::cr2, 11>   dmaen {};
        inline constexpr field_t<tags::cr2, 12>   last {};
    }

    namespace oar1 {
        inline constexpr field_t<tags::oar1, 0>  add0 {};
        inline constexpr field_t<tags::oar1, 1>  add1 {};
        inline constexpr field_t<tags::oar1, 2>  add2 {};
        inline constexpr field_t<tags::oar1, 3>  add3 {};
        inline constexpr field_t<tags::oar1, 4>  add4 {};
        inline constexpr field_t<tags::oar1, 5>  add5 {};
        inline constexpr field_t<tags::oar1, 6>  add6 {};
        inline constexpr field_t<tags::oar1, 7>  add7 {};
        inline constexpr field_t<tags::oar1, 8>  add8 {};
        inline constexpr field_t<tags::oar1, 9>  add9 {};
        inline constexpr field_t<tags::oar1, 15> addmode {};
    }

    namespace oar2 {
        inline constexpr field_t<tags::oar2, 0>    endual {};
        inline constexpr field_t<tags::oar2, 1, 7> add2 {};
    }

    namespace dr {
        inline constexpr field_t<tags::dr, 0, 8> dr {};
    }

    namespace sr1 {
        inline constexpr field_t<tags::sr1, 0>  sb {};
        inline constexpr field_t<tags::sr1, 1>  addr {};
        inline constexpr field_t<tags::sr1, 2>  btf {};
        inline constexpr field_t<tags::sr1, 3>  add10 {};
        inline constexpr field_t<tags::sr1, 4>  stopf {};
        inline constexpr field_t<tags::sr1, 6>  rxne {};
        inline constexpr field_t<tags::sr1, 7>  txe {};
        inline constexpr field_t<tags::sr1, 8>  berr {};
        inline constexpr field_t<tags::sr1, 9>  arlo {};
        inline constexpr field_t<tags::sr1, 10> af {};
        inline constexpr field_t<tags::sr1, 11> ovr {};
        inline constexpr field_t<tags::sr1, 12> pecerr {};
        inline constexpr field_t<tags::sr1, 14> timeout {};
        inline constexpr field_t<tags::sr1, 15> smbalert {};
    }

    namespace sr2 {
        inline constexpr field_t<tags::sr2, 0>    msl {};
        inline constexpr field_t<tags::sr2, 1>    busy {};
        inline constexpr field_t<tags::sr2, 2>    tra {};
        inline constexpr field_t<tags::sr2, 4>    gencall {};
        inline constexpr field_t<tags::sr2, 5>    smbdefault {};
        inline constexpr field_t<tags::sr2, 6>    smbhost {};
        inline constexpr field_t<tags::sr2, 7>    dualf {};
        inline constexpr field_t<tags::sr2, 8, 8> pec {};
    }

    namespace ccr {
        inline constexpr field_t<tags::ccr, 0, 12> ccr {};
        inline constexpr field_t<tags::ccr, 14>    duty {};
        inline constexpr field_t<tags::ccr, 15>    fs {};
    }

    namespace trise {
        inline constexpr field_t<tags::trise, 0, 6> trise {};
    }

    namespace fltr {
        inline constexpr field_t<tags::fltr, 0, 4> dnf {};
        inline constexpr field_t<tags::fltr, 4>    anoff {};
    }
    struct registers_t
    {
        i2c::CR1   CR1;
        i2c::CR2   CR2;
        i2c::OAR1  OAR1;
        i2c::OAR2  OAR2;
        i2c::DR    DR;
        i2c::SR1   SR1;
        i2c::SR2   SR2;
        i2c::CCR   CCR;
        i2c::TRISE TRISE;
        i2c::FLTR  FLTR;
    };

    static_assert(offsetof(registers_t, CR1) == 0x00);
    static_assert(offsetof(registers_t, FLTR) == 0x24);
    static_assert(sizeof(registers_t) == 0x28);

    inline registers_t* const I2C1 = reinterpret_cast<registers_t*>(0x40005400UL);
    inline registers_t* const I2C2 = reinterpret_cast<registers_t*>(0x40005800UL);
    inline registers_t* const I2C3 = reinterpret_cast<registers_t*>(0x40005C00UL);
}

namespace stm32f411::iwdg {
    namespace tags {
        struct kr;
        struct pr;
        struct rlr;
        struct sr;
    }

    using KR = register_t<tags::kr>;
    using PR = register_t<tags::pr>;
    using RLR = register_t<tags::rlr>;
    using SR = read_only_register_t<tags::sr>;

    namespace kr {
        inline constexpr field_t<tags::kr, 0, 16> key {};
    }

    namespace pr {
        inline constexpr field_t<tags::pr, 0, 3> pr {};
    }

    namespace rlr {
        inline constexpr field_t<tags::rlr, 0, 12> rl {};
    }

    namespace sr {
        inline constexpr field_t<tags::sr, 0> pvu {};
        inline constexpr field_t<tags::sr, 1> rvu {};
    }
    struct registers_t
    {
        iwdg::KR  KR;
        iwdg::PR  PR;
        iwdg::RLR RLR;
        iwdg::SR  SR;
    };

    static_assert(offsetof(registers_t, KR) == 0x00);
    static_assert(offsetof(registers_t, SR) == 0x0C);
    static_assert(sizeof(registers_t) == 0x10);

    inline registers_t* const IWDG = reinterpret_cast<registers_t*>(0x40003000UL);
}

namespace stm32f411::pwr {
    namespace tags {
        struct cr;
        struct csr;
    }

    using CR = register_t<tags::cr>;
    using CSR = register_t<tags::csr>;

    namespace cr {
        inline constexpr field_t<tags::cr, 0>     lpds {};
        inline constexpr field_t<tags::cr, 1>     pdds {};
        inline constexpr field_t<tags::cr, 2>     cwuf {};
        inline constexpr field_t<tags::cr, 3>     csbf {};
        inline constexpr field_t<tags::cr, 4>     pvde {};
        inline constexpr field_t<tags::cr, 5, 3>  pls {};
        inline constexpr field_t<tags::cr, 8>     dbp {};
        inline constexpr field_t<tags::cr, 9>     fpds {};
        inline constexpr field_t<tags::cr, 10>    lplvds {};
        inline constexpr field_t<tags::cr, 11>    mrlvds {};
        inline constexpr field_t<tags::cr, 13>    adcdc1 {};
        inline constexpr field_t<tags::cr, 14, 2> vos {};
        inline constexpr field_t<tags::cr, 20>    fmssr {};
        inline constexpr field_t<tags::cr, 21>    fissr {};
    }

    namespace csr {
        inline constexpr field_t<tags::csr, 0>  wuf {};
        inline constexpr field_t<tags::csr, 1>  sbf {};
        inline constexpr field_t<tags::csr, 2>  pvdo {};
        inline constexpr field_t<tags::csr, 3>  brr {};
        inline constexpr field_t<tags::csr, 8>  ewup {};
        inline constexpr field_t<tags::csr, 9>  bre {};
        inline constexpr field_t<tags::csr, 14> vosrdy {};
    }
    struct registers_t
    {
        pwr::CR  CR;
        pwr::CSR CSR;
    };

    static_assert(offsetof(registers_t, CR) == 0x00);
    static_assert(offsetof(registers_t, CSR) == 0x04);
    static_assert(sizeof(registers_t) == 0x08);

    inline registers_t* const PWR = reinterpret_cast<registers_t*>(0x40007000UL);
}

namespace stm32f411::rtc {
    namespace tags {
        struct tr;
        struct dr;
        struct cr;
        struct isr;
        struct prer;
        struct wutr;
        struct calibr;
        struct alrmar;
        struct alrmbr;
        struct wpr;
        struct ssr;
        struct shiftr;
        struct tstr;
        struct tsdr;
        struct tsssr;
        struct calr;
        struct tafcr;
        struct alrmassr;
        struct alrmbssr;
        struct bkpr;
    }

    using TR = register_t<tags::tr>;
    using DR = register_t<tags::dr>;
    using CR = register_t<tags::cr>;
    using ISR = register_t<tags::isr>;
    using PRER = register_t<tags::prer>;
    using WUTR = register_t<tags::wutr>;
    using CALIBR = register_t<tags::calibr>;
    using ALRMAR = register_t<tags::alrmar>;
    using ALRMBR = register_t<tags::alrmbr>;
    using WPR = register_t<tags::wpr>;
    using SSR = register_t<tags::ssr>;
    using SHIFTR = register_t<tags::shiftr>;
    using TSTR = register_t<tags::tstr>;
    using TSDR = register_t<tags::tsdr>;
    using TSSSR = register_t<tags::tsssr>;
    using CALR = register_t<tags::calr>;
    using TAFCR = register_t<tags::tafcr>;
    using ALRMASSR = register_t<tags::alrmassr>;
    using ALRMBSSR = register_t<tags::alrmbssr>;
    using BKPR = register_t<tags::bkpr>;

    namespace tr {
        inline constexpr field_t<tags::tr, 0, 4>  su {};
        inline constexpr field_t<tags::tr, 4, 3>  st {};
        inline constexpr field_t<tags::tr, 8, 4>  mnu {};
        inline constexpr field_t<tags::tr, 12, 3> mnt {};
        inline constexpr field_t<tags::tr, 16, 4> hu {};
        inline constexpr field_t<tags::tr, 20, 2> ht {};
        inline constexpr field_t<tags::tr, 22>    pm {};
    }

    namespace dr {
        inline constexpr field_t<tags::dr, 0, 4>  du {};
        inline constexpr field_t<tags::dr, 4, 2>  dt {};
        inline constexpr field_t<tags::dr, 8, 4>  mu {};
        inline constexpr field_t<tags::dr, 12>    mt {};
        inline constexpr field_t<tags::dr, 13, 3> wdu {};
        inline constexpr field_t<tags::dr, 16, 4> yu {};
        inline constexpr field_t<tags::dr, 20, 4> yt {};
    }

    namespace cr {
        inline constexpr field_t<tags::cr, 0, 3>  wucksel {};
        inline constexpr field_t<tags::cr, 3>     tsedge {};
        inline constexpr field_t<tags::cr, 4>     refckon {};
        inline constexpr field_t<tags::cr, 5>     bypshad {};
        inline constexpr field_t<tags::cr, 6>     fmt {};
        inline constexpr field_t<tags::cr, 7>     dce {};
        inline constexpr field_t<tags::cr, 8>     alrae {};
        inline constexpr field_t<tags::cr, 9>     alrbe {};
        inline constexpr field_t<tags::cr, 10>    wute {};
        inline constexpr field_t<tags::cr, 11>    tse {};
        inline constexpr field_t<tags::cr, 12>    alraie {};
        inline constexpr field_t<tags::cr, 13>    alrbie {};
        inline constexpr field_t<tags::cr, 14>    wutie {};
        inline constexpr field_t<tags::cr, 15>    tsie {};
        inline constexpr field_t<tags::cr, 16>    add1h {};
        inline constexpr field_t<tags::cr, 17>    sub1h {};
        inline constexpr field_t<tags::cr, 18>    bkp {};
        inline constexpr field_t<tags::cr, 19>    cosel {};
        inline constexpr field_t<tags::cr, 20>    pol {};
        inline constexpr field_t<tags::cr, 21, 2> osel {};
        inline constexpr field_t<tags::cr, 23>    coe {};
    }

    namespace isr {
        inline constexpr field_t<tags::isr, 0>  alrawf {};
        inline constexpr field_t<tags::isr, 1>  alrbwf {};
        inline constexpr field_t<tags::isr, 2>  wutwf {};
        inline constexpr field_t<tags::isr, 3>  shpf {};
        inline constexpr field_t<tags::isr, 4>  inits {};
        inline constexpr field_t<tags::isr, 5>  rsf {};
        inline constexpr field_t<tags::isr, 6>  initf {};
        inline constexpr field_t<tags::isr, 7>  init {};
        inline constexpr field_t<tags::isr, 8>  alraf {};
        inline constexpr field_t<tags::isr, 9>  alrbf {};
        inline constexpr field_t<tags::isr, 10> wutf {};
        inline constexpr field_t<tags::isr, 11> tsf {};
        inline constexpr field_t<tags::isr, 12> tsovf {};
        inline constexpr field_t<tags::isr, 13> tamp1f {};
        inline constexpr field_t<tags::isr, 14> tamp2f {};
        inline constexpr field_t<tags::isr, 16> recalpf {};
    }

    namespace prer {
        inline constexpr field_t<tags::prer, 0, 15> prediv_s {};
        inline constexpr field_t<tags::prer, 16, 7> prediv_a {};
    }

    namespace wutr {
        inline constexpr field_t<tags::wutr, 0, 16> wut {};
    }

    namespace calibr {
        inline constexpr field_t<tags::calibr, 0, 5> dc {};
        inline constexpr field_t<tags::calibr, 7>    dcs {};
    }

    namespace alrmar {
        inline constexpr field_t<tags::alrmar, 0, 4>  su {};
        inline constexpr field_t<tags::alrmar, 4, 3>  st {};
        inline constexpr field_t<tags::alrmar, 7>     msk1 {};
        inline constexpr field_t<tags::alrmar, 8, 4>  mnu {};
        inline constexpr field_t<tags::alrmar, 12, 3> mnt {};
        inline constexpr field_t<tags::alrmar, 15>    msk2 {};
        inline constexpr field_t<tags::alrmar, 16, 4> hu {};
        inline constexpr field_t<tags::alrmar, 20, 2> ht {};
        inline constexpr field_t<tags::alrmar, 22>    pm {};
        inline constexpr field_t<tags::alrmar, 23>    msk3 {};
        inline constexpr field_t<tags::alrmar, 24, 4> du {};
        inline constexpr field_t<tags::alrmar, 28, 2> dt {};
        inline constexpr field_t<tags::alrmar, 30>    wdsel {};
        inline constexpr field_t<tags::alrmar, 31>    msk4 {};
    }

    namespace alrmbr {
        inline constexpr field_t<tags::alrmbr, 0, 4>  su {};
        inline constexpr field_t<tags::alrmbr, 4, 3>  st {};
        inline constexpr field_t<tags::alrmbr, 7>     msk1 {};
        inline constexpr field_t<tags::alrmbr, 8, 4>  mnu {};
        inline constexpr field_t<tags::alrmbr, 12, 3> mnt {};
        inline constexpr field_t<tags::alrmbr, 15>    msk2 {};
        inline constexpr field_t<tags::alrmbr, 16, 4> hu {};
        inline constexpr field_t<tags::alrmbr, 20, 2> ht {};
        inline constexpr field_t<tags::alrmbr, 22>    pm {};
        inline constexpr field_t<tags::alrmbr, 23>    msk3 {};
        inline constexpr field_t<tags::alrmbr, 24, 4> du {};
        inline constexpr field_t<tags::alrmbr, 28, 2> dt {};
        inline constexpr field_t<tags::alrmbr, 30>    wdsel {};
        inline constexpr field_t<tags::alrmbr, 31>    msk4 {};
    }

    namespace wpr {
        inline constexpr field_t<tags::wpr, 0, 8> key {};
    }

    namespace ssr {
        inline constexpr field_t<tags::ssr, 0, 16> ss {};
    }

    namespace shiftr {
        inline constexpr field_t<tags::shiftr, 0, 15> subfs {};
        inline constexpr field_t<tags::shiftr, 31>    add1s {};
    }

    namespace tstr {
        inline constexpr field_t<tags::tstr, 0, 4>  su {};
        inline constexpr field_t<tags::tstr, 4, 3>  st {};
        inline constexpr field_t<tags::tstr, 8, 4>  mnu {};
        inline constexpr field_t<tags::tstr, 12, 3> mnt {};
        inline constexpr field_t<tags::tstr, 16, 4> hu {};
        inline constexpr field_t<tags::tstr, 20, 2> ht {};
        inline constexpr field_t<tags::tstr, 22>    pm {};
    }

    namespace tsdr {
        inline constexpr field_t<tags::tsdr, 0, 4>  du {};
        inline constexpr field_t<tags::tsdr, 4, 2>  dt {};
        inline constexpr field_t<tags::tsdr, 8, 4>  mu {};
        inline constexpr field_t<tags::tsdr, 12>    mt {};
        inline constexpr field_t<tags::tsdr, 13, 3> wdu {};
    }

    namespace tsssr {
        inline constexpr field_t<tags::tsssr, 0, 16> ss {};
    }

    namespace calr {
        inline constexpr field_t<tags::calr, 0, 9> calm {};
        inline constexpr field_t<tags::calr, 13>   calw16 {};
        inline constexpr field_t<tags::calr, 14>   calw8 {};
        inline constexpr field_t<tags::calr, 15>   calp {};
    }

    namespace tafcr {
        inline constexpr field_t<tags::tafcr, 0>     tamp1e {};
        inline constexpr field_t<tags::tafcr, 1>     tamp1trg {};
        inline constexpr field_t<tags::tafcr, 2>     tampie {};
        inline constexpr field_t<tags::tafcr, 3>     tamp2e {};
        inline constexpr field_t<tags::tafcr, 4>     tamp2trg {};
        inline constexpr field_t<tags::tafcr, 7>     tampts {};
        inline constexpr field_t<tags::tafcr, 8, 3>  tampfreq {};
        inline constexpr field_t<tags::tafcr, 11, 2> tampflt {};
        inline constexpr field_t<tags::tafcr, 13, 2> tampprch {};
        inline constexpr field_t<tags::tafcr, 15>    tamppudis {};
        inline constexpr field_t<tags::tafcr, 16>    tamp1insel {};
        inline constexpr field_t<tags::tafcr, 17>    tsinsel {};
        inline constexpr field_t<tags::tafcr, 18>    alarmouttype {};
    }

    namespace alrmassr {
        inline constexpr field_t<tags::alrmassr, 0, 15> ss {};
        inline constexpr field_t<tags::alrmassr, 24, 4> maskss {};
    }

    namespace alrmbssr {
        inline constexpr field_t<tags::alrmbssr, 0, 15> ss {};
        inline constexpr field_t<tags::alrmbssr, 24, 4> maskss {};
    }

    namespace bkpr {
        inline constexpr field_t<tags::bkpr, 0, 32> data {};
    }
    struct registers_t
    {
        rtc::TR       TR;
        rtc::DR       DR;
        rtc::CR       CR;
        rtc::ISR      ISR;
        rtc::PRER     PRER;
        rtc::WUTR     WUTR;
        rtc::CALIBR   CALIBR;
        rtc::ALRMAR   ALRMAR;
        rtc::ALRMBR   ALRMBR;
        rtc::WPR      WPR;
        rtc::SSR      SSR;
        rtc::SHIFTR   SHIFTR;
        rtc::TSTR     TSTR;
        rtc::TSDR     TSDR;
        rtc::TSSSR    TSSSR;
        rtc::CALR     CALR;
        rtc::TAFCR    TAFCR;
        rtc::ALRMASSR ALRMASSR;
        rtc::ALRMBSSR ALRMBSSR;
        std::uint32_t reserved_x4C;
        rtc::BKPR     BKPR[20];
    };

    static_assert(offsetof(registers_t, TR) == 0x00);
    static_assert(offsetof(registers_t, ALRMBSSR) == 0x48);
    static_assert(offsetof(registers_t, BKPR) == 0x50);
    static_assert(sizeof(registers_t) == 0xA0);

    inline registers_t* const RTC = reinterpret_cast<registers_t*>(0x40002800UL);
}

namespace stm32f411::sdio {
    namespace tags {
        struct power;
        struct clkcr;
        struct arg;
        struct cmd;
        struct respcmd;
        struct resp1;
        struct resp2;
        struct resp3;
        struct resp4;
        struct dtimer;
        struct dlen;
        struct dctrl;
        struct dcount;
        struct sta;
        struct icr;
        struct mask;
        struct fifocnt;
        struct fifo;
    }

    using POWER = register_t<tags::power>;
    using CLKCR = register_t<tags::clkcr>;
    using ARG = register_t<tags::arg>;
    using CMD = register_t<tags::cmd>;
    using RESPCMD = read_only_register_t<tags::respcmd>;
    using RESP1 = read_only_register_t<tags::resp1>;
    using RESP2 = read_only_register_t<tags::resp2>;
    using RESP3 = read_only_register_t<tags::resp3>;
    using RESP4 = read_only_register_t<tags::resp4>;
    using DTIMER = register_t<tags::dtimer>;
    using DLEN = register_t<tags::dlen>;
    using DCTRL = register_t<tags::dctrl>;
    using DCOUNT = read_only_register_t<tags::dcount>;
    using STA = read_only_register_t<tags::sta>;
    using ICR = register_t<tags::icr>;
    using MASK = register_t<tags::mask>;
    using FIFOCNT = read_only_register_t<tags::fifocnt>;
    using FIFO = register_t<tags::fifo>;

    namespace power {
        inline constexpr field_t<tags::power, 0, 2> pwrctrl {};
    }

    namespace clkcr {
        inline constexpr field_t<tags::clkcr, 0, 8>  clkdiv {};
        inline constexpr field_t<tags::clkcr, 8>     clken {};
        inline constexpr field_t<tags::clkcr, 9>     pwrsav {};
        inline constexpr field_t<tags::clkcr, 10>    bypass {};
        inline constexpr field_t<tags::clkcr, 11, 2> widbus {};
        inline constexpr field_t<tags::clkcr, 13>    negedge {};
        inline constexpr field_t<tags::clkcr, 14>    hwfc_en {};
    }

    namespace arg {
        inline constexpr field_t<tags::arg, 0, 32> cmdarg {};
    }

    namespace cmd {
        inline constexpr field_t<tags::cmd, 0, 6> cmdindex {};
        inline constexpr field_t<tags::cmd, 6, 2> waitresp {};
        inline constexpr field_t<tags::cmd, 8>    waitint {};
        inline constexpr field_t<tags::cmd, 9>    waitpend {};
        inline constexpr field_t<tags::cmd, 10>   cpsmen {};
        inline constexpr field_t<tags::cmd, 11>   sdiosuspend {};
        inline constexpr field_t<tags::cmd, 12>   encmdcompl {};
        inline constexpr field_t<tags::cmd, 13>   nien {};
        inline constexpr field_t<tags::cmd, 14>   ceatacmd {};
    }

    namespace respcmd {
        inline constexpr field_t<tags::respcmd, 0, 6> respcmd {};
    }

    namespace resp1 {
        inline constexpr field_t<tags::resp1, 0, 32> cardstatus1 {};
    }

    namespace resp2 {
        inline constexpr field_t<tags::resp2, 0, 32> cardstatus2 {};
    }

    namespace resp3 {
        inline constexpr field_t<tags::resp3, 0, 32> cardstatus3 {};
    }

    namespace resp4 {
        inline constexpr field_t<tags::resp4, 0, 32> cardstatus4 {};
    }

    namespace dtimer {
        inline constexpr field_t<tags::dtimer, 0, 32> datatime {};
    }

    namespace dlen {
        inline constexpr field_t<tags::dlen, 0, 25> datalength {};
    }

    namespace dctrl {
        inline constexpr field_t<tags::dctrl, 0>    dten {};
        inline constexpr field_t<tags::dctrl, 1>    dtdir {};
        inline constexpr field_t<tags::dctrl, 2>    dtmode {};
        inline constexpr field_t<tags::dctrl, 3>    dmaen {};
        inline constexpr field_t<tags::dctrl, 4, 4> dblocksize {};
        inline constexpr field_t<tags::dctrl, 8>    rwstart {};
        inline constexpr field_t<tags::dctrl, 9>    rwstop {};
        inline constexpr field_t<tags::dctrl, 10>   rwmod {};
        inline constexpr field_t<tags::dctrl, 11>   sdioen {};
    }

    namespace dcount {
        inline constexpr field_t<tags::dcount, 0, 25> datacount {};
    }

    namespace sta {
        inline constexpr field_t<tags::sta, 0>  ccrcfail {};
        inline constexpr field_t<tags::sta, 1>  dcrcfail {};
        inline constexpr field_t<tags::sta, 2>  ctimeout {};
        inline constexpr field_t<tags::sta, 3>  dtimeout {};
        inline constexpr field_t<tags::sta, 4>  txunderr {};
        inline constexpr field_t<tags::sta, 5>  rxoverr {};
        inline constexpr field_t<tags::sta, 6>  cmdrend {};
        inline constexpr field_t<tags::sta, 7>  cmdsent {};
        inline constexpr field_t<tags::sta, 8>  dataend {};
        inline constexpr field_t<tags::sta, 9>  stbiterr {};
        inline constexpr field_t<tags::sta, 10> dbckend {};
        inline constexpr field_t<tags::sta, 11> cmdact {};
        inline constexpr field_t<tags::sta, 12> txact {};
        inline constexpr field_t<tags::sta, 13> rxact {};
        inline constexpr field_t<tags::sta, 14> txfifohe {};
        inline constexpr field_t<tags::sta, 15> rxfifohf {};
        inline constexpr field_t<tags::sta, 16> txfifof {};
        inline constexpr field_t<tags::sta, 17> rxfifof {};
        inline constexpr field_t<tags::sta, 18> txfifoe {};
        inline constexpr field_t<tags::sta, 19> rxfifoe {};
        inline constexpr field_t<tags::sta, 20> txdavl {};
        inline constexpr field_t<tags::sta, 21> rxdavl {};
        inline constexpr field_t<tags::sta, 22> sdioit {};
        inline constexpr field_t<tags::sta, 23> ceataend {};
    }

    namespace icr {
        inline constexpr field_t<tags::icr, 0>  ccrcfailc {};
        inline constexpr field_t<tags::icr, 1>  dcrcfailc {};
        inline constexpr field_t<tags::icr, 2>  ctimeoutc {};
        inline constexpr field_t<tags::icr, 3>  dtimeoutc {};
        inline constexpr field_t<tags::icr, 4>  txunderrc {};
        inline constexpr field_t<tags::icr, 5>  rxoverrc {};
        inline constexpr field_t<tags::icr, 6>  cmdrendc {};
        inline constexpr field_t<tags::icr, 7>  cmdsentc {};
        inline constexpr field_t<tags::icr, 8>  dataendc {};
        inline constexpr field_t<tags::icr, 9>  stbiterrc {};
        inline constexpr field_t<tags::icr, 10> dbckendc {};
        inline constexpr field_t<tags::icr, 22> sdioitc {};
        inline constexpr field_t<tags::icr, 23> ceataendc {};
    }

    namespace mask {
        inline constexpr field_t<tags::mask, 0>  ccrcfailie {};
        inline constexpr field_t<tags::mask, 1>  dcrcfailie {};
        inline constexpr field_t<tags::mask, 2>  ctimeoutie {};
        inline constexpr field_t<tags::mask, 3>  dtimeoutie {};
        inline constexpr field_t<tags::mask, 4>  txunderrie {};
        inline constexpr field_t<tags::mask, 5>  rxoverrie {};
        inline constexpr field_t<tags::mask, 6>  cmdrendie {};
        inline constexpr field_t<tags::mask, 7>  cmdsentie {};
        inline constexpr field_t<tags::mask, 8>  dataendie {};
        inline constexpr field_t<tags::mask, 9>  stbiterrie {};
        inline constexpr field_t<tags::mask, 10> dbckendie {};
        inline constexpr field_t<tags::mask, 11> cmdactie {};
        inline constexpr field_t<tags::mask, 12> txactie {};
        inline constexpr field_t<tags::mask, 13> rxactie {};
        inline constexpr field_t<tags::mask, 14> txfifoheie {};
        inline constexpr field_t<tags::mask, 15> rxfifohfie {};
        inline constexpr field_t<tags::mask, 16> txfifofie {};
        inline constexpr field_t<tags::mask, 17> rxfifofie {};
        inline constexpr field_t<tags::mask, 18> txfifoeie {};
        inline constexpr field_t<tags::mask, 19> rxfifoeie {};
        inline constexpr field_t<tags::mask, 20> txdavlie {};
        inline constexpr field_t<tags::mask, 21> rxdavlie {};
        inline constexpr field_t<tags::mask, 22> sdioitie {};
        inline constexpr field_t<tags::mask, 23> ceataendie {};
    }

    namespace fifocnt {
        inline constexpr field_t<tags::fifocnt, 0, 24> fifocount {};
    }

    namespace fifo {
        inline constexpr field_t<tags::fifo, 0, 32> fifodata {};
    }
    struct registers_t
    {
        sdio::POWER   POWER;
        sdio::CLKCR   CLKCR;
        sdio::ARG     ARG;
        sdio::CMD     CMD;
        sdio::RESPCMD RESPCMD;
        sdio::RESP1   RESP1;
        sdio::RESP2   RESP2;
        sdio::RESP3   RESP3;
        sdio::RESP4   RESP4;
        sdio::DTIMER  DTIMER;
        sdio::DLEN    DLEN;
        sdio::DCTRL   DCTRL;
        sdio::DCOUNT  DCOUNT;
        sdio::STA     STA;
        sdio::ICR     ICR;
        sdio::MASK    MASK;
        std::uint32_t reserved_x40_x44[2];
        sdio::FIFOCNT FIFOCNT;
        std::uint32_t reserved_x4C_x7C[13];
        sdio::FIFO    FIFO;
    };

    static_assert(offsetof(registers_t, POWER) == 0x00);
    static_assert(offsetof(registers_t, FIFOCNT) == 0x48);
    static_assert(offsetof(registers_t, FIFO) == 0x80);
    static_assert(sizeof(registers_t) == 0x84);

    inline registers_t* const SDIO = reinterpret_cast<registers_t*>(0x40012C00UL);
}

namespace stm32f411::spi {
    namespace tags {
        struct cr1;
        struct cr2;
        struct sr;
        struct dr;
        struct crcpr;
        struct rxcrcr;
        struct txcrcr;
        struct i2scfgr;
        struct i2spr;
    }

    using CR1 = register_t<tags::cr1>;
    using CR2 = register_t<tags::cr2>;
    using SR = register_t<tags::sr>;
    using DR = register_t<tags::dr>;
    using CRCPR = register_t<tags::crcpr>;
    using RXCRCR = register_t<tags::rxcrcr>;
    using TXCRCR = register_t<tags::txcrcr>;
    using I2SCFGR = register_t<tags::i2scfgr>;
    using I2SPR = register_t<tags::i2spr>;

    namespace cr1 {
        inline constexpr field_t<tags::cr1, 0>    cpha {};
        inline constexpr field_t<tags::cr1, 1>    cpol {};
        inline constexpr field_t<tags::cr1, 2>    mstr {};
        inline constexpr field_t<tags::cr1, 3, 3> br {};
        inline constexpr field_t<tags::cr1, 6>    spe {};
        inline constexpr field_t<tags::cr1, 7>    lsbfirst {};
        inline constexpr field_t<tags::cr1, 8>    ssi {};
        inline constexpr field_t<tags::cr1, 9>    ssm {};
        inline constexpr field_t<tags::cr1, 10>   rxonly {};
        inline constexpr field_t<tags::cr1, 11>   dff {};
        inline constexpr field_t<tags::cr1, 12>   crcnext {};
        inline constexpr field_t<tags::cr1, 13>   crcen {};
        inline constexpr field_t<tags::cr1, 14>   bidioe {};
        inline constexpr field_t<tags::cr1, 15>   bidimode {};
    }

    namespace cr2 {
        inline constexpr field_t<tags::cr2, 0> rxdmaen {};
        inline constexpr field_t<tags::cr2, 1> txdmaen {};
        inline constexpr field_t<tags::cr2, 2> ssoe {};
        inline constexpr field_t<tags::cr2, 4> frf {};
        inline constexpr field_t<tags::cr2, 5> errie {};
        inline constexpr field_t<tags::cr2, 6> rxneie {};
        inline constexpr field_t<tags::cr2, 7> txeie {};
    }

    namespace sr {
        inline constexpr field_t<tags::sr, 0> rxne {};
        inline constexpr field_t<tags::sr, 1> txe {};
        inline constexpr field_t<tags::sr, 2> chside {};
        inline constexpr field_t<tags::sr, 3> udr {};
        inline constexpr field_t<tags::sr, 4> crcerr {};
        inline constexpr field_t<tags::sr, 5> modf {};
        inline constexpr field_t<tags::sr, 6> ovr {};
        inline constexpr field_t<tags::sr, 7> bsy {};
        inline constexpr field_t<tags::sr, 8> fre {};
    }

    namespace dr {
        inline constexpr field_t<tags::dr, 0, 16> dr {};
    }

    namespace crcpr {
        inline constexpr field_t<tags::crcpr, 0, 16> crcpoly {};
    }

    namespace rxcrcr {
        inline constexpr field_t<tags::rxcrcr, 0, 16> rxcrc {};
    }

    namespace txcrcr {
        inline constexpr field_t<tags::txcrcr, 0, 16> txcrc {};
    }

    namespace i2scfgr {
        inline constexpr field_t<tags::i2scfgr, 0>    chlen {};
        inline constexpr field_t<tags::i2scfgr, 1, 2> datlen {};
        inline constexpr field_t<tags::i2scfgr, 3>    ckpol {};
        inline constexpr field_t<tags::i2scfgr, 4, 2> i2sstd {};
        inline constexpr field_t<tags::i2scfgr, 7>    pcmsync {};
        inline constexpr field_t<tags::i2scfgr, 8, 2> i2scfg {};
        inline constexpr field_t<tags::i2scfgr, 10>   i2se {};
        inline constexpr field_t<tags::i2scfgr, 11>   i2smod {};
    }

    namespace i2spr {
        inline constexpr field_t<tags::i2spr, 0, 8> i2sdiv {};
        inline constexpr field_t<tags::i2spr, 8>    odd {};
        inline constexpr field_t<tags::i2spr, 9>    mckoe {};
    }
    struct registers_t
    {
        spi::CR1     CR1;
        spi::CR2     CR2;
        spi::SR      SR;
        spi::DR      DR;
        spi::CRCPR   CRCPR;
        spi::RXCRCR  RXCRCR;
        spi::TXCRCR  TXCRCR;
        spi::I2SCFGR I2SCFGR;
        spi::I2SPR   I2SPR;
    };

    static_assert(offsetof(registers_t, CR1) == 0x00);
    static_assert(offsetof(registers_t, I2SPR) == 0x20);
    static_assert(sizeof(registers_t) == 0x24);

    inline registers_t* const I2S2EXT = reinterpret_cast<registers_t*>(0x40003400UL);
    inline registers_t* const SPI2 = reinterpret_cast<registers_t*>(0x40003800UL);
    inline registers_t* const SPI3 = reinterpret_cast<registers_t*>(0x40003C00UL);
    inline registers_t* const I2S3EXT = reinterpret_cast<registers_t*>(0x40004000UL);
    inline registers_t* const SPI1 = reinterpret_cast<registers_t*>(0x40013000UL);
    inline registers_t* const SPI4 = reinterpret_cast<registers_t*>(0x40013400UL);
    inline registers_t* const SPI5 = reinterpret_cast<registers_t*>(0x40015000UL);
}

namespace stm32f411::tim {
    namespace tags {
        struct cr1;
        struct cr2;
        struct smcr;
        struct dier;
        struct sr;
        struct egr;
        struct ccmr1;
        struct ccmr2;
        struct ccer;
        struct cnt;
        struct psc;
        struct arr;
        struct rcr;
        struct ccr1;
        struct ccr2;
        struct ccr3;
        struct ccr4;
        struct bdtr;
        struct dcr;
        struct dmar;
        struct or_register;
    }

    using CR1 = register_t<tags::cr1>;
    using CR2 = register_t<tags::cr2>;
    using SMCR = register_t<tags::smcr>;
    using DIER = register_t<tags::dier>;
    using SR = register_t<tags::sr>;
    using EGR = register_t<tags::egr>;
    using CCMR1 = register_t<tags::ccmr1>;
    using CCMR2 = register_t<tags::ccmr2>;
    using CCER = register_t<tags::ccer>;
    using CNT = register_t<tags::cnt>;
    using PSC = register_t<tags::psc>;
    using ARR = register_t<tags::arr>;
    using RCR = register_t<tags::rcr>;
    using CCR1 = register_t<tags::ccr1>;
    using CCR2 = register_t<tags::ccr2>;
    using CCR3 = register_t<tags::ccr3>;
    using CCR4 = register_t<tags::ccr4>;
    using BDTR = register_t<tags::bdtr>;
    using DCR = register_t<tags::dcr>;
    using DMAR = register_t<tags::dmar>;
    using OR = register_t<tags::or_register>;

    namespace cr1 {
        inline constexpr field_t<tags::cr1, 0>    cen {};
        inline constexpr field_t<tags::cr1, 1>    udis {};
        inline constexpr field_t<tags::cr1, 2>    urs {};
        inline constexpr field_t<tags::cr1, 3>    opm {};
        inline constexpr field_t<tags::cr1, 4>    dir {};
        inline constexpr field_t<tags::cr1, 5, 2> cms {};
        inline constexpr field_t<tags::cr1, 7>    arpe {};
        inline constexpr field_t<tags::cr1, 8, 2> ckd {};
    }

    namespace cr2 {
        inline constexpr field_t<tags::cr2, 0>    ccpc {};
        inline constexpr field_t<tags::cr2, 2>    ccus {};
        inline constexpr field_t<tags::cr2, 3>    ccds {};
        inline constexpr field_t<tags::cr2, 4, 3> mms {};
        inline constexpr field_t<tags::cr2, 7>    ti1s {};
        inline constexpr field_t<tags::cr2, 8>    ois1 {};
        inline constexpr field_t<tags::cr2, 9>    ois1n {};
        inline constexpr field_t<tags::cr2, 10>   ois2 {};
        inline constexpr field_t<tags::cr2, 11>   ois2n {};
        inline constexpr field_t<tags::cr2, 12>   ois3 {};
        inline constexpr field_t<tags::cr2, 13>   ois3n {};
        inline constexpr field_t<tags::cr2, 14>   ois4 {};
    }

    namespace smcr {
        inline constexpr field_t<tags::smcr, 0, 3>  sms {};
        inline constexpr field_t<tags::smcr, 4, 3>  ts {};
        inline constexpr field_t<tags::smcr, 7>     msm {};
        inline constexpr field_t<tags::smcr, 8, 4>  etf {};
        inline constexpr field_t<tags::smcr, 12, 2> etps {};
        inline constexpr field_t<tags::smcr, 14>    ece {};
        inline constexpr field_t<tags::smcr, 15>    etp {};
    }

    namespace dier {
        inline constexpr field_t<tags::dier, 0>  uie {};
        inline constexpr field_t<tags::dier, 1>  cc1ie {};
        inline constexpr field_t<tags::dier, 2>  cc2ie {};
        inline constexpr field_t<tags::dier, 3>  cc3ie {};
        inline constexpr field_t<tags::dier, 4>  cc4ie {};
        inline constexpr field_t<tags::dier, 5>  comie {};
        inline constexpr field_t<tags::dier, 6>  tie {};
        inline constexpr field_t<tags::dier, 7>  bie {};
        inline constexpr field_t<tags::dier, 8>  ude {};
        inline constexpr field_t<tags::dier, 9>  cc1de {};
        inline constexpr field_t<tags::dier, 10> cc2de {};
        inline constexpr field_t<tags::dier, 11> cc3de {};
        inline constexpr field_t<tags::dier, 12> cc4de {};
        inline constexpr field_t<tags::dier, 13> comde {};
        inline constexpr field_t<tags::dier, 14> tde {};
    }

    namespace sr {
        inline constexpr field_t<tags::sr, 0>  uif {};
        inline constexpr field_t<tags::sr, 1>  cc1if {};
        inline constexpr field_t<tags::sr, 2>  cc2if {};
        inline constexpr field_t<tags::sr, 3>  cc3if {};
        inline constexpr field_t<tags::sr, 4>  cc4if {};
        inline constexpr field_t<tags::sr, 5>  comif {};
        inline constexpr field_t<tags::sr, 6>  tif {};
        inline constexpr field_t<tags::sr, 7>  bif {};
        inline constexpr field_t<tags::sr, 9>  cc1of {};
        inline constexpr field_t<tags::sr, 10> cc2of {};
        inline constexpr field_t<tags::sr, 11> cc3of {};
        inline constexpr field_t<tags::sr, 12> cc4of {};
    }

    namespace egr {
        inline constexpr field_t<tags::egr, 0> ug {};
        inline constexpr field_t<tags::egr, 1> cc1g {};
        inline constexpr field_t<tags::egr, 2> cc2g {};
        inline constexpr field_t<tags::egr, 3> cc3g {};
        inline constexpr field_t<tags::egr, 4> cc4g {};
        inline constexpr field_t<tags::egr, 5> comg {};
        inline constexpr field_t<tags::egr, 6> tg {};
        inline constexpr field_t<tags::egr, 7> bg {};
    }

    namespace ccmr1 {
        inline constexpr field_t<tags::ccmr1, 0, 2>  cc1s {};
        inline constexpr field_t<tags::ccmr1, 2, 2>  ic1psc {};
        inline constexpr field_t<tags::ccmr1, 2>     oc1fe {};
        inline constexpr field_t<tags::ccmr1, 3>     oc1pe {};
        inline constexpr field_t<tags::ccmr1, 4, 4>  ic1f {};
        inline constexpr field_t<tags::ccmr1, 4, 3>  oc1m {};
        inline constexpr field_t<tags::ccmr1, 7>     oc1ce {};
        inline constexpr field_t<tags::ccmr1, 8, 2>  cc2s {};
        inline constexpr field_t<tags::ccmr1, 10, 2> ic2psc {};
        inline constexpr field_t<tags::ccmr1, 10>    oc2fe {};
        inline constexpr field_t<tags::ccmr1, 11>    oc2pe {};
        inline constexpr field_t<tags::ccmr1, 12, 4> ic2f {};
        inline constexpr field_t<tags::ccmr1, 12, 3> oc2m {};
        inline constexpr field_t<tags::ccmr1, 15>    oc2ce {};
    }

    namespace ccmr2 {
        inline constexpr field_t<tags::ccmr2, 0, 2>  cc3s {};
        inline constexpr field_t<tags::ccmr2, 2, 2>  ic3psc {};
        inline constexpr field_t<tags::ccmr2, 2>     oc3fe {};
        inline constexpr field_t<tags::ccmr2, 3>     oc3pe {};
        inline constexpr field_t<tags::ccmr2, 4, 4>  ic3f {};
        inline constexpr field_t<tags::ccmr2, 4, 3>  oc3m {};
        inline constexpr field_t<tags::ccmr2, 7>     oc3ce {};
        inline constexpr field_t<tags::ccmr2, 8, 2>  cc4s {};
        inline constexpr field_t<tags::ccmr2, 10, 2> ic4psc {};
        inline constexpr field_t<tags::ccmr2, 10>    oc4fe {};
        inline constexpr field_t<tags::ccmr2, 11>    oc4pe {};
        inline constexpr field_t<tags::ccmr2, 12, 4> ic4f {};
        inline constexpr field_t<tags::ccmr2, 12, 3> oc4m {};
        inline constexpr field_t<tags::ccmr2, 15>    oc4ce {};
    }

    namespace ccer {
        inline constexpr field_t<tags::ccer, 0>  cc1e {};
        inline constexpr field_t<tags::ccer, 1>  cc1p {};
        inline constexpr field_t<tags::ccer, 2>  cc1ne {};
        inline constexpr field_t<tags::ccer, 3>  cc1np {};
        inline constexpr field_t<tags::ccer, 4>  cc2e {};
        inline constexpr field_t<tags::ccer, 5>  cc2p {};
        inline constexpr field_t<tags::ccer, 6>  cc2ne {};
        inline constexpr field_t<tags::ccer, 7>  cc2np {};
        inline constexpr field_t<tags::ccer, 8>  cc3e {};
        inline constexpr field_t<tags::ccer, 9>  cc3p {};
        inline constexpr field_t<tags::ccer, 10> cc3ne {};
        inline constexpr field_t<tags::ccer, 11> cc3np {};
        inline constexpr field_t<tags::ccer, 12> cc4e {};
        inline constexpr field_t<tags::ccer, 13> cc4p {};
        inline constexpr field_t<tags::ccer, 15> cc4np {};
    }

    namespace cnt {
        inline constexpr field_t<tags::cnt, 0, 32> cnt {};
    }

    namespace psc {
        inline constexpr field_t<tags::psc, 0, 16> psc {};
    }

    namespace arr {
        inline constexpr field_t<tags::arr, 0, 32> arr {};
    }

    namespace rcr {
        inline constexpr field_t<tags::rcr, 0, 8> rep {};
    }

    namespace ccr1 {
        inline constexpr field_t<tags::ccr1, 0, 16> ccr1 {};
    }

    namespace ccr2 {
        inline constexpr field_t<tags::ccr2, 0, 16> ccr2 {};
    }

    namespace ccr3 {
        inline constexpr field_t<tags::ccr3, 0, 16> ccr3 {};
    }

    namespace ccr4 {
        inline constexpr field_t<tags::ccr4, 0, 16> ccr4 {};
    }

    namespace bdtr {
        inline constexpr field_t<tags::bdtr, 0, 8> dtg {};
        inline constexpr field_t<tags::bdtr, 8, 2> lock {};
        inline constexpr field_t<tags::bdtr, 10>   ossi {};
        inline constexpr field_t<tags::bdtr, 11>   ossr {};
        inline constexpr field_t<tags::bdtr, 12>   bke {};
        inline constexpr field_t<tags::bdtr, 13>   bkp {};
        inline constexpr field_t<tags::bdtr, 14>   aoe {};
        inline constexpr field_t<tags::bdtr, 15>   moe {};
    }

    namespace dcr {
        inline constexpr field_t<tags::dcr, 0, 5> dba {};
        inline constexpr field_t<tags::dcr, 8, 5> dbl {};
    }

    namespace dmar {
        inline constexpr field_t<tags::dmar, 0, 16> dmab {};
    }

    namespace or_reg {
        inline constexpr field_t<tags::or_register, 0, 2>  ti1_rmp {};
        inline constexpr field_t<tags::or_register, 6, 2>  ti4_rmp {};
        inline constexpr field_t<tags::or_register, 10, 2> itr1_rmp {};
    }
    struct registers_t
    {
        tim::CR1   CR1;
        tim::CR2   CR2;
        tim::SMCR  SMCR;
        tim::DIER  DIER;
        tim::SR    SR;
        tim::EGR   EGR;
        tim::CCMR1 CCMR1;
        tim::CCMR2 CCMR2;
        tim::CCER  CCER;
        tim::CNT   CNT;
        tim::PSC   PSC;
        tim::ARR   ARR;
        tim::RCR   RCR;
        tim::CCR1  CCR1;
        tim::CCR2  CCR2;
        tim::CCR3  CCR3;
        tim::CCR4  CCR4;
        tim::BDTR  BDTR;
        tim::DCR   DCR;
        tim::DMAR  DMAR;
        tim::OR    OR;
    };

    static_assert(offsetof(registers_t, CR1) == 0x00);
    static_assert(offsetof(registers_t, OR) == 0x50);
    static_assert(sizeof(registers_t) == 0x54);

    inline registers_t* const TIM2 = reinterpret_cast<registers_t*>(0x40000000UL);
    inline registers_t* const TIM3 = reinterpret_cast<registers_t*>(0x40000400UL);
    inline registers_t* const TIM4 = reinterpret_cast<registers_t*>(0x40000800UL);
    inline registers_t* const TIM5 = reinterpret_cast<registers_t*>(0x40000C00UL);
    inline registers_t* const TIM1 = reinterpret_cast<registers_t*>(0x40010000UL);
    inline registers_t* const TIM9 = reinterpret_cast<registers_t*>(0x40014000UL);
    inline registers_t* const TIM10 = reinterpret_cast<registers_t*>(0x40014400UL);
    inline registers_t* const TIM11 = reinterpret_cast<registers_t*>(0x40014800UL);
}

namespace stm32f411::usart {
    inline constexpr std::uint32_t fpclk = 16'000'000;

    enum class oversampling_t : std::uint8_t
    {
        by_16,
        by_8,
    };

    [[nodiscard]] constexpr std::uint32_t calculate_brr(std::uint32_t  peripheral_clock_hz,
                                                        std::uint32_t  baud_rate,
                                                        oversampling_t mode = oversampling_t::by_16) noexcept
    {
        if (baud_rate == 0)
        {
            return 0;
        }

        const auto divider
            = static_cast<std::uint32_t>((static_cast<std::uint64_t>(peripheral_clock_hz) + baud_rate / 2) / baud_rate);

        if (mode == oversampling_t::by_8)
        {
            if (divider < 8 || divider > 0x7FFF)
            {
                return 0;
            }

            // In OVER8 mode BRR[3] must remain clear; the three fraction
            // bits retain their position while the mantissa shifts left.
            return ((divider & ~0x7UL) << 1) | (divider & 0x7UL);
        }

        return divider >= 16 && divider <= 0xFFFF ? divider : 0;
    }

    [[nodiscard]] constexpr std::uint32_t calculate_brr(std::uint32_t  baud_rate,
                                                        oversampling_t mode = oversampling_t::by_16) noexcept
    {
        return calculate_brr(fpclk, baud_rate, mode);
    }

    static_assert(calculate_brr(16'000'000, 115'200) == 0x008B);
    static_assert(calculate_brr(115'200) == 0x008B);
    static_assert(calculate_brr(16'000'000, 115'200, oversampling_t::by_8) == 0x0113);

    namespace tags {
        struct sr;
        struct dr;
        struct brr;
        struct cr1;
        struct cr2;
        struct cr3;
        struct gtpr;
    }

    using SR = register_t<tags::sr>;
    using DR = register_t<tags::dr>;
    using BRR = register_t<tags::brr>;
    using CR1 = register_t<tags::cr1>;
    using CR2 = register_t<tags::cr2>;
    using CR3 = register_t<tags::cr3>;
    using GTPR = register_t<tags::gtpr>;

    namespace sr {
        inline constexpr field_t<tags::sr, 0> pe {};
        inline constexpr field_t<tags::sr, 1> fe {};
        inline constexpr field_t<tags::sr, 2> nf {};
        inline constexpr field_t<tags::sr, 3> ore {};
        inline constexpr field_t<tags::sr, 4> idle {};
        inline constexpr field_t<tags::sr, 5> rxne {};
        inline constexpr field_t<tags::sr, 6> tc {};
        inline constexpr field_t<tags::sr, 7> txe {};
        inline constexpr field_t<tags::sr, 8> lbd {};
        inline constexpr field_t<tags::sr, 9> cts {};
    }

    namespace dr {
        inline constexpr field_t<tags::dr, 0, 9> data {};
    }

    namespace brr {
        inline constexpr field_t<tags::brr, 0, 4>  fraction {};
        inline constexpr field_t<tags::brr, 4, 12> mantissa {};
    }

    namespace cr1 {
        inline constexpr field_t<tags::cr1, 0>  sbk {};
        inline constexpr field_t<tags::cr1, 1>  rwu {};
        inline constexpr field_t<tags::cr1, 2>  re {};
        inline constexpr field_t<tags::cr1, 3>  te {};
        inline constexpr field_t<tags::cr1, 4>  idleie {};
        inline constexpr field_t<tags::cr1, 5>  rxneie {};
        inline constexpr field_t<tags::cr1, 6>  tcie {};
        inline constexpr field_t<tags::cr1, 7>  txeie {};
        inline constexpr field_t<tags::cr1, 8>  peie {};
        inline constexpr field_t<tags::cr1, 9>  ps {};
        inline constexpr field_t<tags::cr1, 10> pce {};
        inline constexpr field_t<tags::cr1, 11> wake {};
        inline constexpr field_t<tags::cr1, 12> m {};
        inline constexpr field_t<tags::cr1, 13> ue {};
        inline constexpr field_t<tags::cr1, 15> over8 {};
    }

    namespace cr2 {
        inline constexpr field_t<tags::cr2, 0, 4>  add {};
        inline constexpr field_t<tags::cr2, 5>     lbdl {};
        inline constexpr field_t<tags::cr2, 6>     lbdie {};
        inline constexpr field_t<tags::cr2, 8>     lbcl {};
        inline constexpr field_t<tags::cr2, 9>     cpha {};
        inline constexpr field_t<tags::cr2, 10>    cpol {};
        inline constexpr field_t<tags::cr2, 11>    clken {};
        inline constexpr field_t<tags::cr2, 12, 2> stop {};
        inline constexpr field_t<tags::cr2, 14>    linen {};
    }

    namespace cr3 {
        inline constexpr field_t<tags::cr3, 0>  eie {};
        inline constexpr field_t<tags::cr3, 1>  iren {};
        inline constexpr field_t<tags::cr3, 2>  irlp {};
        inline constexpr field_t<tags::cr3, 3>  hdsel {};
        inline constexpr field_t<tags::cr3, 4>  nack {};
        inline constexpr field_t<tags::cr3, 5>  scen {};
        inline constexpr field_t<tags::cr3, 6>  dmar {};
        inline constexpr field_t<tags::cr3, 7>  dmat {};
        inline constexpr field_t<tags::cr3, 8>  rtse {};
        inline constexpr field_t<tags::cr3, 9>  ctse {};
        inline constexpr field_t<tags::cr3, 10> ctsie {};
        inline constexpr field_t<tags::cr3, 11> onebit {};
    }

    namespace gtpr {
        inline constexpr field_t<tags::gtpr, 0, 8> psc {};
        inline constexpr field_t<tags::gtpr, 8, 8> gt {};
    }

    struct registers_t
    {
        usart::SR   SR;
        usart::DR   DR;
        usart::BRR  BRR;
        usart::CR1  CR1;
        usart::CR2  CR2;
        usart::CR3  CR3;
        usart::GTPR GTPR;
    };

    static_assert(offsetof(registers_t, SR) == 0x00);
    static_assert(offsetof(registers_t, GTPR) == 0x18);
    static_assert(sizeof(registers_t) == 0x1C);

    inline constexpr std::uintptr_t usart2_address = 0x40004400UL;
    inline constexpr std::uintptr_t usart1_address = 0x40011000UL;
    inline constexpr std::uintptr_t usart6_address = 0x40011400UL;

    inline constexpr auto usart2_irq_number = irq::irq_t::usart2;

    inline registers_t* const USART2 = reinterpret_cast<registers_t*>(usart2_address);
    inline registers_t* const USART1 = reinterpret_cast<registers_t*>(usart1_address);
    inline registers_t* const USART6 = reinterpret_cast<registers_t*>(usart6_address);

    enum class word_len_t : std::uint8_t
    {
        bits_8,
        bits_9
    };

    enum class parity_t : std::uint8_t
    {
        even,
        odd
    };

    enum class parity_control_t : std::uint8_t
    {
        disabled,
        enabled
    };

    enum class wakeup_method_t : std::uint8_t
    {
        idle,
        address
    };

    struct config_t
    {
        std::uint32_t    baud_rate {};
        bool             rx_en { true };
        bool             tx_en { true };
        bool             idle_intr_en { false };
        bool             rx_dr_not_empty_intr_en { false };
        bool             tx_cplt_intr_en { false };
        bool             tx_dr_empty_intr_en { false };
        bool             parity_error_intr_en { false };
        bool             enable { true };
        parity_t         parity_select { parity_t::even };
        parity_control_t parity_control_en { parity_control_t::disabled };
        wakeup_method_t  wakeup { wakeup_method_t::idle };
        word_len_t       length { word_len_t::bits_8 };
        oversampling_t   sampling { oversampling_t::by_16 };

        [[nodiscard]] constexpr std::uint32_t cr1() const noexcept
        {
            using namespace cr1;

            return re.encode(rx_en) | te.encode(tx_en) | idleie.encode(idle_intr_en)
                   | rxneie.encode(rx_dr_not_empty_intr_en) | tcie.encode(tx_cplt_intr_en)
                   | txeie.encode(tx_dr_empty_intr_en) | peie.encode(parity_error_intr_en) | ue.encode(enable)
                   | ps.encode(std::to_underlying(parity_select)) | pce.encode(std::to_underlying(parity_control_en))
                   | wake.encode(std::to_underlying(wakeup)) | m.encode(std::to_underlying(length))
                   | over8.encode(std::to_underlying(sampling));
        }
    };

    template<auto                       RX,
             auto                       TX,
             std::uintptr_t             Address,
             std::uint32_t              FpClk,
             gpio::alternate_function_t AltFunc,
             irq::irq_t                 IRQNumber>
    struct usart_t
    {
    private:
        registers_t* usart() const noexcept
        {
            return reinterpret_cast<registers_t*>(Address);
        }

    public:
        static constexpr auto irq_number = IRQNumber;

        [[nodiscard]] bool is_cts() const noexcept
        {
            return usart()->SR.read(sr::cts);
        }

        [[nodiscard]] std::uint32_t status() const noexcept
        {
            return usart()->SR.read();
        }

        [[nodiscard]] std::uint8_t read_data_register() const noexcept
        {
            return static_cast<std::uint8_t>(usart()->DR.read());
        }

        void write_data_register(const std::uint8_t value) const noexcept
        {
            // DR reads consume received data. Use the whole-register overload
            // so transmitting never performs a read/modify/write on RX data.
            usart()->DR.write(dr::data.encode(value));
        }

        [[nodiscard]] bool is_txe() const noexcept
        {
            return usart()->SR.read(usart::sr::txe);
        }

        void configure(const config_t& cfg) const noexcept
        {
            usart()->CR1.clear(cr1::ue);

            constexpr gpio::config_t pin_cfg = { .port_mode = gpio::mode_t::alt_func, .alt_func = AltFunc };
            RX.configure(pin_cfg);
            TX.configure(pin_cfg);

            usart()->BRR.write(calculate_brr(FpClk, cfg.baud_rate, cfg.sampling));
            usart()->CR1.write(cfg.cr1());
        }
    };

    template<auto RX, auto TX>
    inline constexpr usart_t<RX, TX, usart2_address, fpclk, gpio::alternate_function_t::AF7, usart2_irq_number> usart2;
}

namespace stm32f411::wwdg {
    namespace tags {
        struct cr;
        struct cfr;
        struct sr;
    }

    using CR = register_t<tags::cr>;
    using CFR = register_t<tags::cfr>;
    using SR = register_t<tags::sr>;

    namespace cr {
        inline constexpr field_t<tags::cr, 0, 7> t {};
        inline constexpr field_t<tags::cr, 7>    wdga {};
    }

    namespace cfr {
        inline constexpr field_t<tags::cfr, 0, 7> w {};
        inline constexpr field_t<tags::cfr, 7, 2> wdgtb {};
        inline constexpr field_t<tags::cfr, 9>    ewi {};
    }

    namespace sr {
        inline constexpr field_t<tags::sr, 0> ewif {};
    }
    struct registers_t
    {
        wwdg::CR  CR;
        wwdg::CFR CFR;
        wwdg::SR  SR;
    };

    static_assert(offsetof(registers_t, CR) == 0x00);
    static_assert(offsetof(registers_t, SR) == 0x08);
    static_assert(sizeof(registers_t) == 0x0C);

    inline registers_t* const WWDG = reinterpret_cast<registers_t*>(0x40002C00UL);
}

namespace stm32f411::usb_otg_fs {
    namespace tags {
        struct gotgctl;
        struct gotgint;
        struct gahbcfg;
        struct gusbcfg;
        struct grstctl;
        struct gintsts;
        struct gintmsk;
        struct grxstsr;
        struct grxstsp;
        struct grxfsiz;
        struct dieptxf0_hnptxfsiz;
        struct hnptxsts;
        struct gccfg;
        struct cid;
        struct hptxfsiz;
        struct dieptxf;
        struct dcfg;
        struct dctl;
        struct dsts;
        struct diepmsk;
        struct doepmsk;
        struct daint;
        struct daintmsk;
        struct dvbusdis;
        struct dvbuspulse;
        struct dthrctl;
        struct diepempmsk;
        struct deachint;
        struct deachmsk;
        struct dinep1msk;
        struct doutep1msk;
        struct diepctl;
        struct diepint;
        struct dieptsiz;
        struct diepdma;
        struct dtxfsts;
        struct doepctl;
        struct doepint;
        struct doeptsiz;
        struct doepdma;
        struct hcfg;
        struct hfir;
        struct hfnum;
        struct hptxsts;
        struct haint;
        struct haintmsk;
        struct hcchar;
        struct hcsplt;
        struct hcint;
        struct hcintmsk;
        struct hctsiz;
        struct hcdma;
        struct hprt;
        struct pcgcctl;
        struct fifo;
    }

    using GOTGCTL = register_t<tags::gotgctl>;
    using GOTGINT = register_t<tags::gotgint>;
    using GAHBCFG = register_t<tags::gahbcfg>;
    using GUSBCFG = register_t<tags::gusbcfg>;
    using GRSTCTL = register_t<tags::grstctl>;
    using GINTSTS = register_t<tags::gintsts>;
    using GINTMSK = register_t<tags::gintmsk>;
    using GRXSTSR = register_t<tags::grxstsr>;
    using GRXSTSP = register_t<tags::grxstsp>;
    using GRXFSIZ = register_t<tags::grxfsiz>;
    using DIEPTXF0_HNPTXFSIZ = register_t<tags::dieptxf0_hnptxfsiz>;
    using HNPTXSTS = register_t<tags::hnptxsts>;
    using GCCFG = register_t<tags::gccfg>;
    using CID = read_only_register_t<tags::cid>;
    using HPTXFSIZ = register_t<tags::hptxfsiz>;
    using DIEPTXF = register_t<tags::dieptxf>;
    using DCFG = register_t<tags::dcfg>;
    using DCTL = register_t<tags::dctl>;
    using DSTS = read_only_register_t<tags::dsts>;
    using DIEPMSK = register_t<tags::diepmsk>;
    using DOEPMSK = register_t<tags::doepmsk>;
    using DAINT = register_t<tags::daint>;
    using DAINTMSK = register_t<tags::daintmsk>;
    using DVBUSDIS = register_t<tags::dvbusdis>;
    using DVBUSPULSE = register_t<tags::dvbuspulse>;
    using DTHRCTL = register_t<tags::dthrctl>;
    using DIEPEMPMSK = register_t<tags::diepempmsk>;
    using DEACHINT = register_t<tags::deachint>;
    using DEACHMSK = register_t<tags::deachmsk>;
    using DINEP1MSK = register_t<tags::dinep1msk>;
    using DOUTEP1MSK = register_t<tags::doutep1msk>;
    using DIEPCTL = register_t<tags::diepctl>;
    using DIEPINT = register_t<tags::diepint>;
    using DIEPTSIZ = register_t<tags::dieptsiz>;
    using DIEPDMA = register_t<tags::diepdma>;
    using DTXFSTS = register_t<tags::dtxfsts>;
    using DOEPCTL = register_t<tags::doepctl>;
    using DOEPINT = register_t<tags::doepint>;
    using DOEPTSIZ = register_t<tags::doeptsiz>;
    using DOEPDMA = register_t<tags::doepdma>;
    using HCFG = register_t<tags::hcfg>;
    using HFIR = register_t<tags::hfir>;
    using HFNUM = register_t<tags::hfnum>;
    using HPTXSTS = register_t<tags::hptxsts>;
    using HAINT = register_t<tags::haint>;
    using HAINTMSK = register_t<tags::haintmsk>;
    using HCCHAR = register_t<tags::hcchar>;
    using HCSPLT = register_t<tags::hcsplt>;
    using HCINT = register_t<tags::hcint>;
    using HCINTMSK = register_t<tags::hcintmsk>;
    using HCTSIZ = register_t<tags::hctsiz>;
    using HCDMA = register_t<tags::hcdma>;
    using HPRT = register_t<tags::hprt>;
    using PCGCCTL = register_t<tags::pcgcctl>;
    using FIFO_REGISTER = register_t<tags::fifo>;

    namespace gotgctl {
        inline constexpr field_t<tags::gotgctl, 0>  srqscs {};
        inline constexpr field_t<tags::gotgctl, 1>  srq {};
        inline constexpr field_t<tags::gotgctl, 8>  hngscs {};
        inline constexpr field_t<tags::gotgctl, 9>  hnprq {};
        inline constexpr field_t<tags::gotgctl, 10> hshnpen {};
        inline constexpr field_t<tags::gotgctl, 11> dhnpen {};
        inline constexpr field_t<tags::gotgctl, 16> cidsts {};
        inline constexpr field_t<tags::gotgctl, 17> dbct {};
        inline constexpr field_t<tags::gotgctl, 18> asvld {};
        inline constexpr field_t<tags::gotgctl, 19> bsvld {};
    }

    namespace gotgint {
        inline constexpr field_t<tags::gotgint, 2>  sedet {};
        inline constexpr field_t<tags::gotgint, 8>  srsschg {};
        inline constexpr field_t<tags::gotgint, 9>  hnsschg {};
        inline constexpr field_t<tags::gotgint, 17> hngdet {};
        inline constexpr field_t<tags::gotgint, 18> adtochg {};
        inline constexpr field_t<tags::gotgint, 19> dbcdne {};
    }

    namespace gahbcfg {
        inline constexpr field_t<tags::gahbcfg, 0>    gint {};
        inline constexpr field_t<tags::gahbcfg, 1, 4> hbstlen {};
        inline constexpr field_t<tags::gahbcfg, 5>    dmaen {};
        inline constexpr field_t<tags::gahbcfg, 7>    txfelvl {};
        inline constexpr field_t<tags::gahbcfg, 8>    ptxfelvl {};
    }

    namespace gusbcfg {
        inline constexpr field_t<tags::gusbcfg, 0, 3>  tocal {};
        inline constexpr field_t<tags::gusbcfg, 6>     physel {};
        inline constexpr field_t<tags::gusbcfg, 8>     srpcap {};
        inline constexpr field_t<tags::gusbcfg, 9>     hnpcap {};
        inline constexpr field_t<tags::gusbcfg, 10, 4> trdt {};
        inline constexpr field_t<tags::gusbcfg, 15>    phylpcs {};
        inline constexpr field_t<tags::gusbcfg, 17>    ulpifsls {};
        inline constexpr field_t<tags::gusbcfg, 18>    ulpiar {};
        inline constexpr field_t<tags::gusbcfg, 19>    ulpicsm {};
        inline constexpr field_t<tags::gusbcfg, 20>    ulpievbusd {};
        inline constexpr field_t<tags::gusbcfg, 21>    ulpievbusi {};
        inline constexpr field_t<tags::gusbcfg, 22>    tsdps {};
        inline constexpr field_t<tags::gusbcfg, 23>    pcci {};
        inline constexpr field_t<tags::gusbcfg, 24>    ptci {};
        inline constexpr field_t<tags::gusbcfg, 25>    ulpiipd {};
        inline constexpr field_t<tags::gusbcfg, 29>    fhmod {};
        inline constexpr field_t<tags::gusbcfg, 30>    fdmod {};
        inline constexpr field_t<tags::gusbcfg, 31>    ctxpkt {};
    }

    namespace grstctl {
        inline constexpr field_t<tags::grstctl, 0>    csrst {};
        inline constexpr field_t<tags::grstctl, 1>    hsrst {};
        inline constexpr field_t<tags::grstctl, 2>    fcrst {};
        inline constexpr field_t<tags::grstctl, 4>    rxfflsh {};
        inline constexpr field_t<tags::grstctl, 5>    txfflsh {};
        inline constexpr field_t<tags::grstctl, 6, 5> txfnum {};
        inline constexpr field_t<tags::grstctl, 30>   dmareq {};
        inline constexpr field_t<tags::grstctl, 31>   ahbidl {};
    }

    namespace gintsts {
        inline constexpr field_t<tags::gintsts, 0>  cmod {};
        inline constexpr field_t<tags::gintsts, 1>  mmis {};
        inline constexpr field_t<tags::gintsts, 2>  otgint {};
        inline constexpr field_t<tags::gintsts, 3>  sof {};
        inline constexpr field_t<tags::gintsts, 4>  rxflvl {};
        inline constexpr field_t<tags::gintsts, 5>  nptxfe {};
        inline constexpr field_t<tags::gintsts, 6>  ginakeff {};
        inline constexpr field_t<tags::gintsts, 7>  boutnakeff {};
        inline constexpr field_t<tags::gintsts, 10> esusp {};
        inline constexpr field_t<tags::gintsts, 11> usbsusp {};
        inline constexpr field_t<tags::gintsts, 12> usbrst {};
        inline constexpr field_t<tags::gintsts, 13> enumdne {};
        inline constexpr field_t<tags::gintsts, 14> isoodrp {};
        inline constexpr field_t<tags::gintsts, 15> eopf {};
        inline constexpr field_t<tags::gintsts, 18> iepint {};
        inline constexpr field_t<tags::gintsts, 19> oepint {};
        inline constexpr field_t<tags::gintsts, 20> iisoixfr {};
        inline constexpr field_t<tags::gintsts, 21> pxfr_incompisoout {};
        inline constexpr field_t<tags::gintsts, 22> datafsusp {};
        inline constexpr field_t<tags::gintsts, 24> hprtint {};
        inline constexpr field_t<tags::gintsts, 25> hcint {};
        inline constexpr field_t<tags::gintsts, 26> ptxfe {};
        inline constexpr field_t<tags::gintsts, 28> cidschg {};
        inline constexpr field_t<tags::gintsts, 29> discint {};
        inline constexpr field_t<tags::gintsts, 30> srqint {};
        inline constexpr field_t<tags::gintsts, 31> wkuint {};
    }

    namespace gintmsk {
        inline constexpr field_t<tags::gintmsk, 1>  mmism {};
        inline constexpr field_t<tags::gintmsk, 2>  otgint {};
        inline constexpr field_t<tags::gintmsk, 3>  sofm {};
        inline constexpr field_t<tags::gintmsk, 4>  rxflvlm {};
        inline constexpr field_t<tags::gintmsk, 5>  nptxfem {};
        inline constexpr field_t<tags::gintmsk, 6>  ginakeffm {};
        inline constexpr field_t<tags::gintmsk, 7>  gonakeffm {};
        inline constexpr field_t<tags::gintmsk, 10> esuspm {};
        inline constexpr field_t<tags::gintmsk, 11> usbsuspm {};
        inline constexpr field_t<tags::gintmsk, 12> usbrst {};
        inline constexpr field_t<tags::gintmsk, 13> enumdnem {};
        inline constexpr field_t<tags::gintmsk, 14> isoodrpm {};
        inline constexpr field_t<tags::gintmsk, 15> eopfm {};
        inline constexpr field_t<tags::gintmsk, 17> epmism {};
        inline constexpr field_t<tags::gintmsk, 18> iepint {};
        inline constexpr field_t<tags::gintmsk, 19> oepint {};
        inline constexpr field_t<tags::gintmsk, 20> iisoixfrm {};
        inline constexpr field_t<tags::gintmsk, 21> pxfrm_iisooxfrm {};
        inline constexpr field_t<tags::gintmsk, 22> fsuspm {};
        inline constexpr field_t<tags::gintmsk, 24> prtim {};
        inline constexpr field_t<tags::gintmsk, 25> hcim {};
        inline constexpr field_t<tags::gintmsk, 26> ptxfem {};
        inline constexpr field_t<tags::gintmsk, 28> cidschgm {};
        inline constexpr field_t<tags::gintmsk, 29> discint {};
        inline constexpr field_t<tags::gintmsk, 30> srqim {};
        inline constexpr field_t<tags::gintmsk, 31> wuim {};
    }

    namespace grxstsr {
        inline constexpr field_t<tags::grxstsr, 0, 4>  epnum {};
        inline constexpr field_t<tags::grxstsr, 4, 11> bcnt {};
        inline constexpr field_t<tags::grxstsr, 15, 2> dpid {};
        inline constexpr field_t<tags::grxstsr, 17, 4> pktsts {};
    }

    namespace grxstsp {
        inline constexpr field_t<tags::grxstsp, 0, 4>  epnum {};
        inline constexpr field_t<tags::grxstsp, 4, 11> bcnt {};
        inline constexpr field_t<tags::grxstsp, 15, 2> dpid {};
        inline constexpr field_t<tags::grxstsp, 17, 4> pktsts {};
    }

    namespace grxfsiz {
        inline constexpr field_t<tags::grxfsiz, 0, 16> rxfd {};
    }

    namespace dieptxf0_hnptxfsiz {
        inline constexpr field_t<tags::dieptxf0_hnptxfsiz, 0, 16>  nptxfsa {};
        inline constexpr field_t<tags::dieptxf0_hnptxfsiz, 0, 16>  tx0fsa {};
        inline constexpr field_t<tags::dieptxf0_hnptxfsiz, 16, 16> nptxfd {};
        inline constexpr field_t<tags::dieptxf0_hnptxfsiz, 16, 16> tx0fd {};
    }

    namespace hnptxsts {
        inline constexpr field_t<tags::hnptxsts, 0, 16> nptxfsav {};
        inline constexpr field_t<tags::hnptxsts, 16, 8> nptqxsav {};
        inline constexpr field_t<tags::hnptxsts, 24, 7> nptxqtop {};
    }

    namespace gccfg {
        inline constexpr field_t<tags::gccfg, 16> pwrdwn {};
        inline constexpr field_t<tags::gccfg, 17> i2cpaden {};
        inline constexpr field_t<tags::gccfg, 18> vbusasen {};
        inline constexpr field_t<tags::gccfg, 19> vbusbsen {};
        inline constexpr field_t<tags::gccfg, 20> sofouten {};
        inline constexpr field_t<tags::gccfg, 21> novbussens {};
    }

    namespace cid {
        inline constexpr field_t<tags::cid, 0, 32> product_id {};
    }

    namespace hptxfsiz {
        inline constexpr field_t<tags::hptxfsiz, 0, 16>  ptxsa {};
        inline constexpr field_t<tags::hptxfsiz, 16, 16> ptxfd {};
    }

    namespace dieptxf {
        inline constexpr field_t<tags::dieptxf, 0, 16>  ineptxsa {};
        inline constexpr field_t<tags::dieptxf, 16, 16> ineptxfd {};
    }

    namespace dcfg {
        inline constexpr field_t<tags::dcfg, 0, 2>  dspd {};
        inline constexpr field_t<tags::dcfg, 2>     nzlsohsk {};
        inline constexpr field_t<tags::dcfg, 4, 7>  dad {};
        inline constexpr field_t<tags::dcfg, 11, 2> pfivl {};
        inline constexpr field_t<tags::dcfg, 14>    xcvrdly {};
        inline constexpr field_t<tags::dcfg, 15>    erratim {};
        inline constexpr field_t<tags::dcfg, 24, 2> perschivl {};
    }

    namespace dctl {
        inline constexpr field_t<tags::dctl, 0>    rwusig {};
        inline constexpr field_t<tags::dctl, 1>    sdis {};
        inline constexpr field_t<tags::dctl, 2>    ginsts {};
        inline constexpr field_t<tags::dctl, 3>    gonsts {};
        inline constexpr field_t<tags::dctl, 4, 3> tctl {};
        inline constexpr field_t<tags::dctl, 7>    sginak {};
        inline constexpr field_t<tags::dctl, 8>    cginak {};
        inline constexpr field_t<tags::dctl, 9>    sgonak {};
        inline constexpr field_t<tags::dctl, 10>   cgonak {};
        inline constexpr field_t<tags::dctl, 11>   poprgdne {};
    }

    namespace dsts {
        inline constexpr field_t<tags::dsts, 0>     suspsts {};
        inline constexpr field_t<tags::dsts, 1, 2>  enumspd {};
        inline constexpr field_t<tags::dsts, 3>     eerr {};
        inline constexpr field_t<tags::dsts, 8, 14> fnsof {};
    }

    namespace diepmsk {
        inline constexpr field_t<tags::diepmsk, 0>  xfrcm {};
        inline constexpr field_t<tags::diepmsk, 1>  epdm {};
        inline constexpr field_t<tags::diepmsk, 3>  tom {};
        inline constexpr field_t<tags::diepmsk, 4>  ittxfemsk {};
        inline constexpr field_t<tags::diepmsk, 5>  inepnmm {};
        inline constexpr field_t<tags::diepmsk, 6>  inepnem {};
        inline constexpr field_t<tags::diepmsk, 8>  txfurm {};
        inline constexpr field_t<tags::diepmsk, 13> nakm {};
    }

    namespace doepmsk {
        inline constexpr field_t<tags::doepmsk, 0>  xfrcm {};
        inline constexpr field_t<tags::doepmsk, 1>  epdm {};
        inline constexpr field_t<tags::doepmsk, 2>  ahberrm {};
        inline constexpr field_t<tags::doepmsk, 3>  stupm {};
        inline constexpr field_t<tags::doepmsk, 4>  otepdm {};
        inline constexpr field_t<tags::doepmsk, 5>  otepsprm {};
        inline constexpr field_t<tags::doepmsk, 6>  b2bstup {};
        inline constexpr field_t<tags::doepmsk, 8>  opem {};
        inline constexpr field_t<tags::doepmsk, 9>  boim {};
        inline constexpr field_t<tags::doepmsk, 12> berrm {};
        inline constexpr field_t<tags::doepmsk, 13> nakm {};
        inline constexpr field_t<tags::doepmsk, 14> nyetm {};
    }

    namespace daint {
        inline constexpr field_t<tags::daint, 0, 16>  iepint {};
        inline constexpr field_t<tags::daint, 16, 16> oepint {};
    }

    namespace daintmsk {
        inline constexpr field_t<tags::daintmsk, 0, 16>  iepm {};
        inline constexpr field_t<tags::daintmsk, 16, 16> oepm {};
    }

    namespace dvbusdis {
        inline constexpr field_t<tags::dvbusdis, 0, 16> vbusdt {};
    }

    namespace dvbuspulse {
        inline constexpr field_t<tags::dvbuspulse, 0, 12> dvbusp {};
    }

    namespace dthrctl {
        inline constexpr field_t<tags::dthrctl, 0>     nonisothren {};
        inline constexpr field_t<tags::dthrctl, 1>     isothren {};
        inline constexpr field_t<tags::dthrctl, 2, 9>  txthrlen {};
        inline constexpr field_t<tags::dthrctl, 16>    rxthren {};
        inline constexpr field_t<tags::dthrctl, 17, 9> rxthrlen {};
        inline constexpr field_t<tags::dthrctl, 27>    arpen {};
    }

    namespace diepempmsk {
        inline constexpr field_t<tags::diepempmsk, 0, 16> ineptxfem {};
    }

    namespace deachint {
        inline constexpr field_t<tags::deachint, 1>  iep1int {};
        inline constexpr field_t<tags::deachint, 17> oep1int {};
    }

    namespace deachmsk {
        inline constexpr field_t<tags::deachmsk, 1>  iep1intm {};
        inline constexpr field_t<tags::deachmsk, 17> oep1intm {};
    }

    namespace dinep1msk {
        inline constexpr field_t<tags::dinep1msk, 0>  xfrcm {};
        inline constexpr field_t<tags::dinep1msk, 1>  epdm {};
        inline constexpr field_t<tags::dinep1msk, 3>  tom {};
        inline constexpr field_t<tags::dinep1msk, 4>  ittxfemsk {};
        inline constexpr field_t<tags::dinep1msk, 5>  inepnmm {};
        inline constexpr field_t<tags::dinep1msk, 6>  inepnem {};
        inline constexpr field_t<tags::dinep1msk, 8>  txfurm {};
        inline constexpr field_t<tags::dinep1msk, 9>  bim {};
        inline constexpr field_t<tags::dinep1msk, 13> nakm {};
    }

    namespace doutep1msk {
        inline constexpr field_t<tags::doutep1msk, 0>  xfrcm {};
        inline constexpr field_t<tags::doutep1msk, 1>  epdm {};
        inline constexpr field_t<tags::doutep1msk, 3>  tom {};
        inline constexpr field_t<tags::doutep1msk, 4>  ittxfemsk {};
        inline constexpr field_t<tags::doutep1msk, 5>  inepnmm {};
        inline constexpr field_t<tags::doutep1msk, 6>  inepnem {};
        inline constexpr field_t<tags::doutep1msk, 8>  txfurm {};
        inline constexpr field_t<tags::doutep1msk, 9>  bim {};
        inline constexpr field_t<tags::doutep1msk, 12> berrm {};
        inline constexpr field_t<tags::doutep1msk, 13> nakm {};
        inline constexpr field_t<tags::doutep1msk, 14> nyetm {};
    }

    namespace diepctl {
        inline constexpr field_t<tags::diepctl, 0, 11> mpsiz {};
        inline constexpr field_t<tags::diepctl, 15>    usbaep {};
        inline constexpr field_t<tags::diepctl, 16>    eonum_dpid {};
        inline constexpr field_t<tags::diepctl, 17>    naksts {};
        inline constexpr field_t<tags::diepctl, 18, 2> eptyp {};
        inline constexpr field_t<tags::diepctl, 21>    stall {};
        inline constexpr field_t<tags::diepctl, 22, 4> txfnum {};
        inline constexpr field_t<tags::diepctl, 26>    cnak {};
        inline constexpr field_t<tags::diepctl, 27>    snak {};
        inline constexpr field_t<tags::diepctl, 28>    sd0pid_sevnfrm {};
        inline constexpr field_t<tags::diepctl, 29>    soddfrm {};
        inline constexpr field_t<tags::diepctl, 30>    epdis {};
        inline constexpr field_t<tags::diepctl, 31>    epena {};
    }

    namespace diepint {
        inline constexpr field_t<tags::diepint, 0>  xfrc {};
        inline constexpr field_t<tags::diepint, 1>  epdisd {};
        inline constexpr field_t<tags::diepint, 2>  ahberr {};
        inline constexpr field_t<tags::diepint, 3>  toc {};
        inline constexpr field_t<tags::diepint, 4>  ittxfe {};
        inline constexpr field_t<tags::diepint, 5>  inepnm {};
        inline constexpr field_t<tags::diepint, 6>  inepne {};
        inline constexpr field_t<tags::diepint, 7>  txfe {};
        inline constexpr field_t<tags::diepint, 8>  txfifoudrn {};
        inline constexpr field_t<tags::diepint, 9>  bna {};
        inline constexpr field_t<tags::diepint, 11> pktdrpsts {};
        inline constexpr field_t<tags::diepint, 12> berr {};
        inline constexpr field_t<tags::diepint, 13> nak {};
    }

    namespace dieptsiz {
        inline constexpr field_t<tags::dieptsiz, 0, 19>  xfrsiz {};
        inline constexpr field_t<tags::dieptsiz, 19, 10> pktcnt {};
        inline constexpr field_t<tags::dieptsiz, 29, 2>  mulcnt {};
    }

    namespace diepdma {
        inline constexpr field_t<tags::diepdma, 0, 32> dmaaddr {};
    }

    namespace dtxfsts {
        inline constexpr field_t<tags::dtxfsts, 0, 16> ineptfsav {};
    }

    namespace doepctl {
        inline constexpr field_t<tags::doepctl, 0, 11> mpsiz {};
        inline constexpr field_t<tags::doepctl, 15>    usbaep {};
        inline constexpr field_t<tags::doepctl, 17>    naksts {};
        inline constexpr field_t<tags::doepctl, 18, 2> eptyp {};
        inline constexpr field_t<tags::doepctl, 20>    snpm {};
        inline constexpr field_t<tags::doepctl, 21>    stall {};
        inline constexpr field_t<tags::doepctl, 26>    cnak {};
        inline constexpr field_t<tags::doepctl, 27>    snak {};
        inline constexpr field_t<tags::doepctl, 28>    sd0pid_sevnfrm {};
        inline constexpr field_t<tags::doepctl, 29>    soddfrm {};
        inline constexpr field_t<tags::doepctl, 30>    epdis {};
        inline constexpr field_t<tags::doepctl, 31>    epena {};
    }

    namespace doepint {
        inline constexpr field_t<tags::doepint, 0>  xfrc {};
        inline constexpr field_t<tags::doepint, 1>  epdisd {};
        inline constexpr field_t<tags::doepint, 2>  ahberr {};
        inline constexpr field_t<tags::doepint, 3>  stup {};
        inline constexpr field_t<tags::doepint, 4>  otepdis {};
        inline constexpr field_t<tags::doepint, 5>  otepspr {};
        inline constexpr field_t<tags::doepint, 6>  b2bstup {};
        inline constexpr field_t<tags::doepint, 8>  outpkterr {};
        inline constexpr field_t<tags::doepint, 13> nak {};
        inline constexpr field_t<tags::doepint, 14> nyet {};
        inline constexpr field_t<tags::doepint, 15> stpktrx {};
    }

    namespace doeptsiz {
        inline constexpr field_t<tags::doeptsiz, 0, 19>  xfrsiz {};
        inline constexpr field_t<tags::doeptsiz, 19, 10> pktcnt {};
        inline constexpr field_t<tags::doeptsiz, 29, 2>  stupcnt {};
    }

    namespace doepdma {
        inline constexpr field_t<tags::doepdma, 0, 32> dmaaddr {};
    }

    namespace hcfg {
        inline constexpr field_t<tags::hcfg, 0, 2> fslspcs {};
        inline constexpr field_t<tags::hcfg, 2>    fslss {};
    }

    namespace hfir {
        inline constexpr field_t<tags::hfir, 0, 16> frivl {};
    }

    namespace hfnum {
        inline constexpr field_t<tags::hfnum, 0, 16>  frnum {};
        inline constexpr field_t<tags::hfnum, 16, 16> ftrem {};
    }

    namespace hptxsts {
        inline constexpr field_t<tags::hptxsts, 0, 16> ptxfsavl {};
        inline constexpr field_t<tags::hptxsts, 16, 8> ptxqsav {};
        inline constexpr field_t<tags::hptxsts, 24, 8> ptxqtop {};
    }

    namespace haint {
        inline constexpr field_t<tags::haint, 0, 16> haint {};
    }

    namespace haintmsk {
        inline constexpr field_t<tags::haintmsk, 0, 16> haintm {};
    }

    namespace hcchar {
        inline constexpr field_t<tags::hcchar, 0, 11> mpsiz {};
        inline constexpr field_t<tags::hcchar, 11, 4> epnum {};
        inline constexpr field_t<tags::hcchar, 15>    epdir {};
        inline constexpr field_t<tags::hcchar, 17>    lsdev {};
        inline constexpr field_t<tags::hcchar, 18, 2> eptyp {};
        inline constexpr field_t<tags::hcchar, 20, 2> mc {};
        inline constexpr field_t<tags::hcchar, 22, 7> dad {};
        inline constexpr field_t<tags::hcchar, 29>    oddfrm {};
        inline constexpr field_t<tags::hcchar, 30>    chdis {};
        inline constexpr field_t<tags::hcchar, 31>    chena {};
    }

    namespace hcsplt {
        inline constexpr field_t<tags::hcsplt, 0, 7>  prtaddr {};
        inline constexpr field_t<tags::hcsplt, 7, 7>  hubaddr {};
        inline constexpr field_t<tags::hcsplt, 14, 2> xactpos {};
        inline constexpr field_t<tags::hcsplt, 16>    complsplt {};
        inline constexpr field_t<tags::hcsplt, 31>    spliten {};
    }

    namespace hcint {
        inline constexpr field_t<tags::hcint, 0>  xfrc {};
        inline constexpr field_t<tags::hcint, 1>  chh {};
        inline constexpr field_t<tags::hcint, 2>  ahberr {};
        inline constexpr field_t<tags::hcint, 3>  stall {};
        inline constexpr field_t<tags::hcint, 4>  nak {};
        inline constexpr field_t<tags::hcint, 5>  ack {};
        inline constexpr field_t<tags::hcint, 6>  nyet {};
        inline constexpr field_t<tags::hcint, 7>  txerr {};
        inline constexpr field_t<tags::hcint, 8>  bberr {};
        inline constexpr field_t<tags::hcint, 9>  frmor {};
        inline constexpr field_t<tags::hcint, 10> dterr {};
    }

    namespace hcintmsk {
        inline constexpr field_t<tags::hcintmsk, 0>  xfrcm {};
        inline constexpr field_t<tags::hcintmsk, 1>  chhm {};
        inline constexpr field_t<tags::hcintmsk, 2>  ahberr {};
        inline constexpr field_t<tags::hcintmsk, 3>  stallm {};
        inline constexpr field_t<tags::hcintmsk, 4>  nakm {};
        inline constexpr field_t<tags::hcintmsk, 5>  ackm {};
        inline constexpr field_t<tags::hcintmsk, 6>  nyet {};
        inline constexpr field_t<tags::hcintmsk, 7>  txerrm {};
        inline constexpr field_t<tags::hcintmsk, 8>  bberrm {};
        inline constexpr field_t<tags::hcintmsk, 9>  frmorm {};
        inline constexpr field_t<tags::hcintmsk, 10> dterrm {};
    }

    namespace hctsiz {
        inline constexpr field_t<tags::hctsiz, 0, 19>  xfrsiz {};
        inline constexpr field_t<tags::hctsiz, 19, 10> pktcnt {};
        inline constexpr field_t<tags::hctsiz, 29, 2>  dpid {};
        inline constexpr field_t<tags::hctsiz, 31>     doping {};
    }

    namespace hcdma {
        inline constexpr field_t<tags::hcdma, 0, 32> dmaaddr {};
    }

    namespace hprt {
        inline constexpr field_t<tags::hprt, 0>     pcsts {};
        inline constexpr field_t<tags::hprt, 1>     pcdet {};
        inline constexpr field_t<tags::hprt, 2>     pena {};
        inline constexpr field_t<tags::hprt, 3>     penchng {};
        inline constexpr field_t<tags::hprt, 4>     poca {};
        inline constexpr field_t<tags::hprt, 5>     pocchng {};
        inline constexpr field_t<tags::hprt, 6>     pres {};
        inline constexpr field_t<tags::hprt, 7>     psusp {};
        inline constexpr field_t<tags::hprt, 8>     prst {};
        inline constexpr field_t<tags::hprt, 10, 2> plsts {};
        inline constexpr field_t<tags::hprt, 12>    ppwr {};
        inline constexpr field_t<tags::hprt, 13, 4> ptctl {};
        inline constexpr field_t<tags::hprt, 17, 2> pspd {};
    }

    namespace pcgcctl {
        inline constexpr field_t<tags::pcgcctl, 0> stopclk {};
        inline constexpr field_t<tags::pcgcctl, 1> gateclk {};
        inline constexpr field_t<tags::pcgcctl, 4> physusp {};
    }

    namespace fifo {
        inline constexpr field_t<tags::fifo, 0, 32> data {};
    }
    struct global_registers_t
    {
        usb_otg_fs::GOTGCTL            GOTGCTL;
        usb_otg_fs::GOTGINT            GOTGINT;
        usb_otg_fs::GAHBCFG            GAHBCFG;
        usb_otg_fs::GUSBCFG            GUSBCFG;
        usb_otg_fs::GRSTCTL            GRSTCTL;
        usb_otg_fs::GINTSTS            GINTSTS;
        usb_otg_fs::GINTMSK            GINTMSK;
        usb_otg_fs::GRXSTSR            GRXSTSR;
        usb_otg_fs::GRXSTSP            GRXSTSP;
        usb_otg_fs::GRXFSIZ            GRXFSIZ;
        usb_otg_fs::DIEPTXF0_HNPTXFSIZ DIEPTXF0_HNPTXFSIZ;
        usb_otg_fs::HNPTXSTS           HNPTXSTS;
        std::uint32_t                  reserved_x30_x34[2];
        usb_otg_fs::GCCFG              GCCFG;
        usb_otg_fs::CID                CID;
        std::uint32_t                  reserved_x40_xFC[48];
        usb_otg_fs::HPTXFSIZ           HPTXFSIZ;
        usb_otg_fs::DIEPTXF            DIEPTXF[15];
    };

    struct device_registers_t
    {
        usb_otg_fs::DCFG       DCFG;
        usb_otg_fs::DCTL       DCTL;
        usb_otg_fs::DSTS       DSTS;
        std::uint32_t          reserved_x0C;
        usb_otg_fs::DIEPMSK    DIEPMSK;
        usb_otg_fs::DOEPMSK    DOEPMSK;
        usb_otg_fs::DAINT      DAINT;
        usb_otg_fs::DAINTMSK   DAINTMSK;
        std::uint32_t          reserved_x20_x24[2];
        usb_otg_fs::DVBUSDIS   DVBUSDIS;
        usb_otg_fs::DVBUSPULSE DVBUSPULSE;
        usb_otg_fs::DTHRCTL    DTHRCTL;
        usb_otg_fs::DIEPEMPMSK DIEPEMPMSK;
        usb_otg_fs::DEACHINT   DEACHINT;
        usb_otg_fs::DEACHMSK   DEACHMSK;
        std::uint32_t          reserved_x40;
        usb_otg_fs::DINEP1MSK  DINEP1MSK;
        std::uint32_t          reserved_x48_x80[15];
        usb_otg_fs::DOUTEP1MSK DOUTEP1MSK;
    };

    struct in_endpoint_registers_t
    {
        usb_otg_fs::DIEPCTL  DIEPCTL;
        std::uint32_t        reserved_x04;
        usb_otg_fs::DIEPINT  DIEPINT;
        std::uint32_t        reserved_x0C;
        usb_otg_fs::DIEPTSIZ DIEPTSIZ;
        usb_otg_fs::DIEPDMA  DIEPDMA;
        usb_otg_fs::DTXFSTS  DTXFSTS;
        std::uint32_t        reserved_x1C;
    };

    struct out_endpoint_registers_t
    {
        usb_otg_fs::DOEPCTL  DOEPCTL;
        std::uint32_t        reserved_x04;
        usb_otg_fs::DOEPINT  DOEPINT;
        std::uint32_t        reserved_x0C;
        usb_otg_fs::DOEPTSIZ DOEPTSIZ;
        usb_otg_fs::DOEPDMA  DOEPDMA;
        std::uint32_t        reserved_x18_x1C[2];
    };

    struct host_registers_t
    {
        usb_otg_fs::HCFG     HCFG;
        usb_otg_fs::HFIR     HFIR;
        usb_otg_fs::HFNUM    HFNUM;
        std::uint32_t        reserved_x0C;
        usb_otg_fs::HPTXSTS  HPTXSTS;
        usb_otg_fs::HAINT    HAINT;
        usb_otg_fs::HAINTMSK HAINTMSK;
    };

    struct host_channel_registers_t
    {
        usb_otg_fs::HCCHAR   HCCHAR;
        usb_otg_fs::HCSPLT   HCSPLT;
        usb_otg_fs::HCINT    HCINT;
        usb_otg_fs::HCINTMSK HCINTMSK;
        usb_otg_fs::HCTSIZ   HCTSIZ;
        usb_otg_fs::HCDMA    HCDMA;
        std::uint32_t        reserved_x18_x1C[2];
    };

    static_assert(offsetof(global_registers_t, GOTGCTL) == 0x00);
    static_assert(offsetof(global_registers_t, GCCFG) == 0x38);
    static_assert(offsetof(global_registers_t, HPTXFSIZ) == 0x100);
    static_assert(offsetof(global_registers_t, DIEPTXF) == 0x104);
    static_assert(sizeof(global_registers_t) == 0x140);
    static_assert(offsetof(device_registers_t, DCFG) == 0x00);
    static_assert(offsetof(device_registers_t, DOUTEP1MSK) == 0x84);
    static_assert(sizeof(device_registers_t) == 0x88);
    static_assert(offsetof(in_endpoint_registers_t, DIEPCTL) == 0x00);
    static_assert(offsetof(in_endpoint_registers_t, DTXFSTS) == 0x18);
    static_assert(sizeof(in_endpoint_registers_t) == 0x20);
    static_assert(offsetof(out_endpoint_registers_t, DOEPCTL) == 0x00);
    static_assert(offsetof(out_endpoint_registers_t, DOEPDMA) == 0x14);
    static_assert(sizeof(out_endpoint_registers_t) == 0x20);
    static_assert(offsetof(host_registers_t, HCFG) == 0x00);
    static_assert(offsetof(host_registers_t, HAINTMSK) == 0x18);
    static_assert(sizeof(host_registers_t) == 0x1C);
    static_assert(offsetof(host_channel_registers_t, HCCHAR) == 0x00);
    static_assert(offsetof(host_channel_registers_t, HCDMA) == 0x14);
    static_assert(sizeof(host_channel_registers_t) == 0x20);

    inline global_registers_t* const  GLOBAL = reinterpret_cast<global_registers_t*>(0x50000000UL);
    inline host_registers_t* const    HOST = reinterpret_cast<host_registers_t*>(0x50000400UL);
    inline HPRT* const                HOST_PORT = reinterpret_cast<HPRT*>(0x50000440UL);
    inline device_registers_t* const  DEVICE = reinterpret_cast<device_registers_t*>(0x50000800UL);
    inline usb_otg_fs::PCGCCTL* const POWER_CLOCK = reinterpret_cast<usb_otg_fs::PCGCCTL*>(0x50000E00UL);

    template<std::uint32_t Endpoint>
    requires(Endpoint < 4)
    inline in_endpoint_registers_t* const IN_ENDPOINT
        = reinterpret_cast<in_endpoint_registers_t*>(0x50000900UL + Endpoint * 0x20UL);

    template<std::uint32_t Endpoint>
    requires(Endpoint < 4)
    inline out_endpoint_registers_t* const OUT_ENDPOINT
        = reinterpret_cast<out_endpoint_registers_t*>(0x50000B00UL + Endpoint * 0x20UL);

    template<std::uint32_t Channel>
    requires(Channel < 8)
    inline host_channel_registers_t* const HOST_CHANNEL
        = reinterpret_cast<host_channel_registers_t*>(0x50000500UL + Channel * 0x20UL);

    template<std::uint32_t Fifo>
    requires(Fifo < 4)
    inline FIFO_REGISTER* const FIFO = reinterpret_cast<FIFO_REGISTER*>(0x50001000UL + Fifo * 0x1000UL);
}
