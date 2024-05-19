#include <stdio.h>


#define __ALIGN_KERNEL(x, a) __ALIGN_KERNEL_MASK(x, (typeof(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask) (((x) + (mask)) & ~(mask))
int main()
{
    printf("0x123, 8:%x\n", __ALIGN_KERNEL(0x123, 8));
}
