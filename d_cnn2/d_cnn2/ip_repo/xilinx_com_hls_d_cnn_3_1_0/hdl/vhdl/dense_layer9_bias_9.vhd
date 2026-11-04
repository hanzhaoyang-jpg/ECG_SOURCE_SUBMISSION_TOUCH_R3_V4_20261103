-- ==============================================================
-- File generated on Wed Dec 17 16:39:23 +0800 2025
-- Vivado(TM) HLS - High-Level Synthesis from C, C++ and SystemC v2018.3 (64-bit)
-- SW Build 2405991 on Thu Dec  6 23:38:27 MST 2018
-- IP Build 2404404 on Fri Dec  7 01:43:56 MST 2018
-- Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
-- ==============================================================
library ieee; 
use ieee.std_logic_1164.all; 
use ieee.std_logic_unsigned.all;

entity dense_layer9_bias_9_rom is 
    generic(
             DWIDTH     : integer := 12; 
             AWIDTH     : integer := 6; 
             MEM_SIZE    : integer := 64
    ); 
    port (
          addr0      : in std_logic_vector(AWIDTH-1 downto 0); 
          ce0       : in std_logic; 
          q0         : out std_logic_vector(DWIDTH-1 downto 0);
          clk       : in std_logic
    ); 
end entity; 


architecture rtl of dense_layer9_bias_9_rom is 

signal addr0_tmp : std_logic_vector(AWIDTH-1 downto 0); 
type mem_array is array (0 to MEM_SIZE-1) of std_logic_vector (DWIDTH-1 downto 0); 
signal mem : mem_array := (
    0 => "111110011010", 1 => "000101010101", 2 => "000001100110", 
    3 => "000110100110", 4 => "000111101010", 5 => "000101010010", 
    6 => "111000001111", 7 => "000001111001", 8 => "111000101011", 
    9 => "111111101100", 10 => "001100001110", 11 => "000010011100", 
    12 => "111111111101", 13 => "001000000110", 14 => "111001111111", 
    15 => "001000010001", 16 => "000011100101", 17 => "010011000111", 
    18 => "111000110111", 19 => "000101110110", 20 => "111110110010", 
    21 => "000100011010", 22 => "000010111101", 23 => "000100001011", 
    24 => "010001100101", 25 => "011111000001", 26 => "010100001000", 
    27 => "000010001111", 28 => "000101110100", 29 => "111000111011", 
    30 => "111001010011", 31 => "000001011111", 32 => "111111001100", 
    33 => "000011101101", 34 => "111100001000", 35 => "000001111111", 
    36 => "111101000110", 37 => "010001001011", 38 => "000010000101", 
    39 => "000001110000", 40 => "000110101000", 41 => "000010010000", 
    42 => "000001110100", 43 => "000001110111", 44 => "110111100000", 
    45 => "110111100101", 46 => "111001011001", 47 => "111011101111", 
    48 => "000100011100", 49 => "111110011000", 50 => "110111011110", 
    51 => "010100010000", 52 => "111011101010", 53 => "111100000111", 
    54 => "000110010001", 55 => "111010100101", 56 => "111110011100", 
    57 => "000011101100", 58 => "111011011100", 59 => "000010110000", 
    60 => "001001001111", 61 => "111111111100", 62 => "001001000101", 
    63 => "001111110000" );

attribute syn_rom_style : string;
attribute syn_rom_style of mem : signal is "select_rom";
attribute ROM_STYLE : string;
attribute ROM_STYLE of mem : signal is "distributed";

begin 


memory_access_guard_0: process (addr0) 
begin
      addr0_tmp <= addr0;
--synthesis translate_off
      if (CONV_INTEGER(addr0) > mem_size-1) then
           addr0_tmp <= (others => '0');
      else 
           addr0_tmp <= addr0;
      end if;
--synthesis translate_on
end process;

p_rom_access: process (clk)  
begin 
    if (clk'event and clk = '1') then
        if (ce0 = '1') then 
            q0 <= mem(CONV_INTEGER(addr0_tmp)); 
        end if;
    end if;
end process;

end rtl;

Library IEEE;
use IEEE.std_logic_1164.all;

entity dense_layer9_bias_9 is
    generic (
        DataWidth : INTEGER := 12;
        AddressRange : INTEGER := 64;
        AddressWidth : INTEGER := 6);
    port (
        reset : IN STD_LOGIC;
        clk : IN STD_LOGIC;
        address0 : IN STD_LOGIC_VECTOR(AddressWidth - 1 DOWNTO 0);
        ce0 : IN STD_LOGIC;
        q0 : OUT STD_LOGIC_VECTOR(DataWidth - 1 DOWNTO 0));
end entity;

architecture arch of dense_layer9_bias_9 is
    component dense_layer9_bias_9_rom is
        port (
            clk : IN STD_LOGIC;
            addr0 : IN STD_LOGIC_VECTOR;
            ce0 : IN STD_LOGIC;
            q0 : OUT STD_LOGIC_VECTOR);
    end component;



begin
    dense_layer9_bias_9_rom_U :  component dense_layer9_bias_9_rom
    port map (
        clk => clk,
        addr0 => address0,
        ce0 => ce0,
        q0 => q0);

end architecture;


