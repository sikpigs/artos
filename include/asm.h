#pragma once

#define dmb() __asm volatile("dmb" ::: "memory")