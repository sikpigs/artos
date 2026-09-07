#include <board.h>
#include <build_config.h>

#ifdef CONFIG_ARTOS
#include <artos.h>
#endif
#ifdef CONFIG_MONITOR
#include <monitor/monitor.h>
#endif

int main()
{
    board::init();
#ifdef CONFIG_ARTOS
    artos::kernel::kernel_t kernel {};
#endif
#ifdef CONFIG_MONITOR
    monitor::run();
#endif
    for (;;)
        __asm volatile("wfi" ::: "memory");
}
