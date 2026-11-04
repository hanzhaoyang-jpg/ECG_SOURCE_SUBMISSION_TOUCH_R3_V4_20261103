# =============================================================================
# 1. LCD 显示接口 (来自 d_cnn2)
# =============================================================================
set_property -dict {PACKAGE_PIN Y18 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[0]}]
set_property -dict {PACKAGE_PIN Y19 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[1]}]
set_property -dict {PACKAGE_PIN W20 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[2]}]
set_property -dict {PACKAGE_PIN V20 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[3]}]
set_property -dict {PACKAGE_PIN U14 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[4]}]
set_property -dict {PACKAGE_PIN U15 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[5]}]
set_property -dict {PACKAGE_PIN T20 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[6]}]
set_property -dict {PACKAGE_PIN U20 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[7]}]
set_property -dict {PACKAGE_PIN W14 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[8]}]
set_property -dict {PACKAGE_PIN Y14 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[9]}]
set_property -dict {PACKAGE_PIN N15 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[10]}]
set_property -dict {PACKAGE_PIN N16 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[11]}]
set_property -dict {PACKAGE_PIN V16 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[12]}]
set_property -dict {PACKAGE_PIN W16 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[13]}]
set_property -dict {PACKAGE_PIN W18 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[14]}]
set_property -dict {PACKAGE_PIN W19 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[15]}]
set_property -dict {PACKAGE_PIN T10 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[16]}]
set_property -dict {PACKAGE_PIN T11 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[17]}]
set_property -dict {PACKAGE_PIN P14 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[18]}]
set_property -dict {PACKAGE_PIN R14 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[19]}]
set_property -dict {PACKAGE_PIN V13 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[20]}]
set_property -dict {PACKAGE_PIN U13 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[21]}]
set_property -dict {PACKAGE_PIN G15 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[22]}]
set_property -dict {PACKAGE_PIN H15 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb_0_tri_io[23]}]
set_property -dict {PACKAGE_PIN U17 IOSTANDARD LVCMOS33} [get_ports lcd_0_hs]
set_property -dict {PACKAGE_PIN P20 IOSTANDARD LVCMOS33} [get_ports lcd_0_vs]
set_property -dict {PACKAGE_PIN N20 IOSTANDARD LVCMOS33} [get_ports lcd_0_de]
set_property -dict {PACKAGE_PIN Y16 IOSTANDARD LVCMOS33} [get_ports lcd_bl_0]
set_property -dict {PACKAGE_PIN T16 IOSTANDARD LVCMOS33} [get_ports lcd_0_clk]

# =============================================================================
# 2. 原有的 GPIO (保留 J18 给 LED/GPIO)
# =============================================================================
set_property PACKAGE_PIN J18 [get_ports {gpio_rtl_0_tri_o[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_rtl_0_tri_o[0]}]
set_property PACKAGE_PIN H18 [get_ports {gpio_rtl_0_tri_o[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {gpio_rtl_0_tri_o[1]}]

# =============================================================================
# 3. CN121 SPI 接口与 EMIO GPIO (来自 yaoyao)
# =============================================================================
set_property PACKAGE_PIN C20 [get_ports CN121_CS]
set_property IOSTANDARD LVCMOS33 [get_ports CN121_CS]
set_property PACKAGE_PIN U19 [get_ports CN121_SCLK]
set_property IOSTANDARD LVCMOS33 [get_ports CN121_SCLK]
set_property PACKAGE_PIN B19 [get_ports CN121_MOSI]
set_property IOSTANDARD LVCMOS33 [get_ports CN121_MOSI]
set_property PACKAGE_PIN K18 [get_ports CN121_MISO]
set_property IOSTANDARD LVCMOS33 [get_ports CN121_MISO]

set_property PACKAGE_PIN P19 [get_ports {GPIO_0_0_tri_io[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_0_tri_io[0]}]
set_property PACKAGE_PIN N18 [get_ports {GPIO_0_0_tri_io[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_0_tri_io[1]}]

# =============================================================================
# 4. AD9280 采集接口 (来自 yaoyao)
# =============================================================================
set_property PACKAGE_PIN D20 [get_ports ad_sclk_0]
set_property IOSTANDARD LVCMOS33 [get_ports ad_sclk_0]

# 【注意】otr 信号已从 J18 挪到 L17 (不关键引脚)
set_property PACKAGE_PIN L17 [get_ports ad_otr_0]
set_property IOSTANDARD LVCMOS33 [get_ports ad_otr_0]

# LOD
set_property PACKAGE_PIN J16 [get_ports lod_in_0]
set_property IOSTANDARD LVCMOS33 [get_ports lod_in_0]

# AD9280 数据线 (8位)
set_property PACKAGE_PIN T17 [get_ports {ad_data_in_0[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ad_data_in_0[0]}]
set_property PACKAGE_PIN P15 [get_ports {ad_data_in_0[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ad_data_in_0[1]}]
set_property PACKAGE_PIN V15 [get_ports {ad_data_in_0[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ad_data_in_0[2]}]
set_property PACKAGE_PIN T12 [get_ports {ad_data_in_0[3]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ad_data_in_0[3]}]
set_property PACKAGE_PIN V12 [get_ports {ad_data_in_0[4]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ad_data_in_0[4]}]
set_property PACKAGE_PIN T14 [get_ports {ad_data_in_0[5]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ad_data_in_0[5]}]
set_property PACKAGE_PIN M18 [get_ports {ad_data_in_0[6]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ad_data_in_0[6]}]
set_property PACKAGE_PIN J20 [get_ports {ad_data_in_0[7]}]
set_property IOSTANDARD LVCMOS33 [get_ports {ad_data_in_0[7]}]

# =============================================================================
# 5. 时钟约束
# =============================================================================
set_false_path -from [get_ports {ad_data_in_0[*]}] -to [get_clocks *]
set_false_path -from [get_ports ad_otr_0] -to [get_clocks *]