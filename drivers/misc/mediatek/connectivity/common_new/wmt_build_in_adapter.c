/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
/*#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__*/
#include <linux/kernel.h>

#include "wmt_build_in_adapter.h"

/*device tree mode*/
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqreturn.h>
#include <linux/of_address.h>
#endif

#include <linux/interrupt.h>

#define CONNADP_LOG_LOUD    4
#define CONNADP_LOG_DBG     3
#define CONNADP_LOG_INFO    2
#define CONNADP_LOG_WARN    1
#define CONNADP_LOG_ERR     0

/*******************************************************************************
 * Connsys adaptation layer logging utility
 ******************************************************************************/
static unsigned int gConnAdpDbgLvl = CONNADP_LOG_INFO;

#define CONNADP_LOUD_FUNC(fmt, arg...) \
do { \
	if (gConnAdpDbgLvl >= CONNADP_LOG_LOUD) \
		pr_info("[L]%s:"  fmt, __func__, ##arg); \
} while (0)
#define CONNADP_DBG_FUNC(fmt, arg...) \
do { \
	if (gConnAdpDbgLvl >= CONNADP_LOG_DBG) \
		pr_info("[D]%s:"  fmt, __func__, ##arg); \
} while (0)
#define CONNADP_INFO_FUNC(fmt, arg...)  \
do { \
	if (gConnAdpDbgLvl >= CONNADP_LOG_INFO) \
		pr_info("[I]%s:"  fmt, __func__, ##arg); \
} while (0)
#define CONNADP_WARN_FUNC(fmt, arg...) \
do { \
	if (gConnAdpDbgLvl >= CONNADP_LOG_WARN) \
		pr_info("[W]%s:"  fmt, __func__, ##arg); \
} while (0)
#define CONNADP_ERR_FUNC(fmt, arg...) \
do { \
	if (gConnAdpDbgLvl >= CONNADP_LOG_ERR) \
		pr_info("[E]%s(%d):"  fmt, __func__, __LINE__, ##arg); \
} while (0)


/*******************************************************************************
 * Bridging from platform -> wmt_drv.ko
 ******************************************************************************/
static struct wmt_platform_bridge bridge;

void wmt_export_platform_bridge_register(struct wmt_platform_bridge *cb)
{
	if (unlikely(!cb))
		return;
	bridge.thermal_query_cb = cb->thermal_query_cb;
#if MTK_WCN_CMB_FOR_SDIO_1V_AUTOK
	bridge.autok_cb = cb->autok_cb;
#endif
	CONNADP_INFO_FUNC("\n");
}
EXPORT_SYMBOL(wmt_export_platform_bridge_register);

void wmt_export_platform_bridge_unregister(void)
{
	memset(&bridge, 0, sizeof(struct wmt_platform_bridge));
	CONNADP_INFO_FUNC("\n");
}
EXPORT_SYMBOL(wmt_export_platform_bridge_unregister);

/*******************************************************************************
 * SDIO integration with platform MMC driver
 ******************************************************************************/

static void mtk_wcn_cmb_sdio_disable_eirq(void);

static atomic_t sdio_claim_irq_enable_flag;
static atomic_t irq_enable_flag;

unsigned int wifi_irq = 0xffffffff;
EXPORT_SYMBOL(wifi_irq);

pm_callback_t mtk_wcn_cmb_sdio_pm_cb;
EXPORT_SYMBOL(mtk_wcn_cmb_sdio_pm_cb);

void *mtk_wcn_cmb_sdio_pm_data;
EXPORT_SYMBOL(mtk_wcn_cmb_sdio_pm_data);

static int _mtk_wcn_sdio_irq_flag_set(int flag)
{
	if (flag != 0)
		atomic_set(&sdio_claim_irq_enable_flag, 1);
	else
		atomic_set(&sdio_claim_irq_enable_flag, 0);

	CONNADP_DBG_FUNC("sdio_claim_irq_enable_flag:%d\n", atomic_read(&sdio_claim_irq_enable_flag));

	return atomic_read(&sdio_claim_irq_enable_flag);
}

int wmt_export_mtk_wcn_sdio_irq_flag_set(int flag)
{
	return _mtk_wcn_sdio_irq_flag_set(flag);
}
EXPORT_SYMBOL(wmt_export_mtk_wcn_sdio_irq_flag_set);

static void mtk_wcn_cmb_sdio_disable_eirq(void)
{
	if (!atomic_read(&irq_enable_flag))
		CONNADP_DBG_FUNC("wifi eint has been disabled!\n");
	else {
		if (wifi_irq != 0xfffffff) {
			disable_irq_nosync(wifi_irq);
			CONNADP_DBG_FUNC("disable WIFI EINT irq %d !!\n", wifi_irq);
		}
		atomic_set(&irq_enable_flag, 0);
	}
}

void wmt_export_mtk_wcn_cmb_sdio_disable_eirq(void)
{
	mtk_wcn_cmb_sdio_disable_eirq();
}
EXPORT_SYMBOL(wmt_export_mtk_wcn_cmb_sdio_disable_eirq);

