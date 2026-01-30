/*
 * Copyright (c) 2018-2026 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GUI_INFO_H_
#define _GUI_INFO_H_

#include <libs/lvgl/lvgl.h>

int dump_cal0();

//=========================================================
//  ASAP: NAND info, Quick lockpick. (gui_emummc_tools.c)
//=========================================================
lv_res_t _create_window_sdcard_info_status(lv_obj_t *btn);
lv_res_t _create_window_emmc_info_status(lv_obj_t *btn);
//========================================
//  ASAP: Battery info, HW info. (gui.c)
//========================================
lv_res_t _create_window_battery_status(lv_obj_t *btn);
lv_res_t _create_window_hw_info_status(lv_obj_t *btn);
//=========================================================

#endif
