/*
 * Copyright 2010-2011 Calxeda, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <ahci.h>
#include <netdev.h>
#include <scsi.h>

#include <asm/sizes.h>
#include <asm/io.h>

#define HB_SREG_A9_PWR_REQ		0xfff3cf00
#define HB_SREG_A9_BOOT_SRC_STAT	0xfff3cf04
#define HB_SREG_A9_PWRDOM_STAT		0xfff3cf20
#define HB_SYSRAM_OPP_TABLE_BASE	0xfff8f000
#define HB_OPP_VERSION			0

#define HB_PWR_SUSPEND			0
#define HB_PWR_SOFT_RESET		1
#define HB_PWR_HARD_RESET		2
#define HB_PWR_SHUTDOWN			3

#define PWRDOM_STAT_SATA		0x80000000
#define PWRDOM_STAT_PCI			0x40000000
#define PWRDOM_STAT_EMMC		0x20000000

DECLARE_GLOBAL_DATA_PTR;

/*
 * Miscellaneous platform dependent initialisations
 */
int board_init(void)
{
	icache_enable();

	return 0;
}

/* We know all the init functions have been run now */
int board_eth_init(bd_t *bis)
{
	int rc = 0;

#ifdef CONFIG_CALXEDA_XGMAC
	rc += calxedaxgmac_initialize(0, 0xfff50000);
	rc += calxedaxgmac_initialize(1, 0xfff51000);
#endif
	return rc;
}

int misc_init_r(void)
{
	char envbuffer[16];
	u32 boot_choice;
	u32 reg = readl(HB_SREG_A9_PWRDOM_STAT);

	if (reg & PWRDOM_STAT_SATA) {
		ahci_init(0xffe08000);
		scsi_scan(1);
	}

	boot_choice = readl(HB_SREG_A9_BOOT_SRC_STAT) & 0xff;
	sprintf(envbuffer, "bootcmd%d", boot_choice);
	if (getenv(envbuffer)) {
		sprintf(envbuffer, "run bootcmd%d", boot_choice);
		setenv("bootcmd", envbuffer);
	} else
		setenv("bootcmd", "");

	return 0;
}

int dram_init(void)
{
	gd->ram_size = SZ_512M;
	return 0;
}

void dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size =  PHYS_SDRAM_1_SIZE;
}

#if defined(CONFIG_OF_BOARD_SETUP)
struct a9_opp {
	unsigned int freq_hz;
	unsigned int volt_mv;
};


void ft_board_setup(void *fdt, bd_t *bd)
{
	const char disabled[] = "disabled";
	u32 *opp_table = HB_SYSRAM_OPP_TABLE_BASE;
	u32 reg = readl(HB_SREG_A9_PWRDOM_STAT);

	if (!(reg & PWRDOM_STAT_SATA))
		do_fixup_by_compat(fdt, "calxeda,hb-ahci", "status", disabled, sizeof(disabled), 1);

	if (!(reg & PWRDOM_STAT_EMMC))
		do_fixup_by_compat(fdt, "calxeda,hb-sdhci", "status", disabled, sizeof(disabled), 1);

	if ((opp_table[0] >> 16) == HB_OPP_VERSION) {
		u32 dtb_table[2*10];
		u32 i;
		u32 num_opps = opp_table[0] & 0xff;
		for (i = 0; i < num_opps; i++) {
			dtb_table[2 * i] = cpu_to_be32(opp_table[3 + 3 * i]);
			dtb_table[2 * i + 1] =
					cpu_to_be32(opp_table[2 + 3 * i]);
		}
		fdt_find_and_setprop(fdt, "/cpus/cpu@0", "transition-latency",
			cpu_to_be32(opp_table[1]), 4, 1);
		fdt_find_and_setprop(fdt, "/cpus/cpu@0", "operating-points",
			dtb_table, 8 * num_opps, 1);
	}
}
#endif

void reset_cpu(ulong addr)
{
	writel(HB_PWR_HARD_RESET, HB_SREG_A9_PWR_REQ);
	writeb(0x3, 0xfff10008);
	/* older compilers don't understand wfi instr, so hardcode it */
	asm(" .word 0xe320f003");
}
