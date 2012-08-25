#
# Makefile for TVIN.
#

obj-y += tvin_format_table.o tvin_frontend.o tvin_global.o

obj-$(CONFIG_TVIN_AFE) += tvin_afe.o
tvin_afe-objs := tvafe_adc.o tvafe_cvd.o tvafe_general.o tvafe.o

obj-$(CONFIG_TVIN_HDMI) += tvin_hdmirx.o
tvin_hdmirx-objs := hdmirx.o hdmirx_hw.o hdmirx_cec.o

obj-$(CONFIG_TVIN_VDIN) += tvin_vdin.o
tvin_vdin-objs := vdin_vf.o vdin.o vdin_ctl.o vdin_sm.o vdin_canvas.o

