/*
 * Copyright (c) 2010 Michael Huang
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cpufreq.h>

#define DRIVER_AUTHOR "Michael Huang <coolbho3000@gmail.com>"
#define DRIVER_DESCRIPTION "HTC Vision overclock module"
#define DRIVER_VERSION "1.1"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

#define IOMEM(x)        ((void __force __iomem *)(x))
#define MSM_CLK_CTL_BASE IOMEM(0xf8005000)
#define PLL2_L_VAL_ADDR  (MSM_CLK_CTL_BASE + 0x33c)

/*
   Default parameters and addresses are for 1017.6MHz @ 1200mV and T-Mobile G2
   HTC kernel 2.6.32.17. Module params must be customized for other kernels and
   speeds.
*/

static uint pll2_l_val = 53 ; /* MSM8x55 spec. 19.2MHz * 53 = 1017.6MHz */
static uint vdd_mv = 1200; /* 1200mV */
static uint vdd_raw = 242; /* 1200mV. VDD_RAW(x) = 224+(x-750)/25 */
static uint acpu_freq_tbl_addr = 0xc055e1a0;
static uint perflock_notifier_call_addr = 0xc006c974;

module_param(pll2_l_val, uint, 0444);
module_param(vdd_mv, uint, 0444);
module_param(vdd_raw, uint, 0444);
module_param(acpu_freq_tbl_addr, uint, 0444);
module_param(perflock_notifier_call_addr, uint, 0444);

MODULE_PARM_DESC(pll2_l_val, "PLL2 L value");
MODULE_PARM_DESC(vdd_mv, "vdd_mv value");
MODULE_PARM_DESC(vdd_raw, "vdd_raw value");
MODULE_PARM_DESC(acpu_freq_tbl_addr, "Memory address of acpu_freq_tbl");
MODULE_PARM_DESC(perflock_notifier_call_addr, "Memory address of perflock_notifier_call");

static struct cpufreq_policy *policy;

struct clkctl_acpu_speed {
	unsigned int	acpu_clk_khz;
	int		src;
	unsigned int	acpu_src_sel;
	unsigned int	acpu_src_div;
	unsigned int	axi_clk_khz;
	unsigned int	vdd_mv;
	unsigned int	vdd_raw;
	unsigned long	lpj;
};

static struct cpufreq_frequency_table *freq_table;
static struct clkctl_acpu_speed *acpu_freq_tbl;

/*
   Machine code for perflock disabler.
   Perflock disabler is mandatory because the perflock driver is hard coded
   with the old freq values.
*/
static char perflock_code[] __initdata =
		"\x0d\xc0\xa0\xe1" //mov r12, sp
		"\x00\xd8\x2d\xe9" //stmdb sp!, {r11, r12, lr, pc}
		"\x04\xb0\x4c\xe2" //sub r11, r12, #4
		"\x00\x00\xa0\xe3" //mov r0, #0
		"\x00\xa8\x9d\xe8" //ldmia sp, {r11, sp, pc}
		;

static int __init vision_oc_init(void)
{
	int frequency_max;
	frequency_max = (19200 * pll2_l_val);
	policy = cpufreq_cpu_get(smp_processor_id());	
	acpu_freq_tbl = (void*) acpu_freq_tbl_addr;
	freq_table = cpufreq_frequency_get_table(smp_processor_id());

	printk(KERN_INFO "vision_oc: %s version %s\n", DRIVER_DESCRIPTION, DRIVER_VERSION);
	printk(KERN_INFO "vision_oc: by %s\n", DRIVER_AUTHOR);
	printk(KERN_INFO "vision_oc: disabling perflock\n");

	memcpy((void*) perflock_notifier_call_addr, &perflock_code, sizeof(perflock_code));
	
	printk(KERN_INFO "vision_oc: fixing cpufreq tables\n");	

	acpu_freq_tbl[8].acpu_clk_khz = frequency_max;
  	acpu_freq_tbl[8].vdd_mv = vdd_mv;
   	acpu_freq_tbl[8].vdd_raw = vdd_raw;
  	freq_table[3].frequency = frequency_max;

	printk(KERN_INFO "vision_oc: writing pll2 L val\n");

	writel(pll2_l_val, PLL2_L_VAL_ADDR);
	udelay(50);

	policy->cpuinfo.min_freq = freq_table[0].frequency;
	policy->cpuinfo.max_freq = frequency_max;
	policy->min = freq_table[0].frequency;
	policy->max = frequency_max;

	return 0;
}

static void __exit vision_oc_exit(void)
{
	printk(KERN_INFO "vision_oc: vision overclock module unloaded\n");
}

module_init(vision_oc_init);
module_exit(vision_oc_exit);
