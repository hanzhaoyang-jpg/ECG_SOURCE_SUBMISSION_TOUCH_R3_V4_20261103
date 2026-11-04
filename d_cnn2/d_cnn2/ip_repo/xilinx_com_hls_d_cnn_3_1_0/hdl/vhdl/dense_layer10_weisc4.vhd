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

entity dense_layer10_weisc4_rom is 
    generic(
             DWIDTH     : integer := 8; 
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


architecture rtl of dense_layer10_weisc4_rom is 

signal addr0_tmp : std_logic_vector(AWIDTH-1 downto 0); 
type mem_array is array (0 to MEM_SIZE-1) of std_logic_vector (DWIDTH-1 downto 0); 
signal mem : mem_array := (
    0 => "10011101", 1 => "01000011", 2 => "00100011", 3 => "01010101", 
    4 => "00101101", 5 => "00110001", 6 => "11111011", 7 => "10110001", 
    8 => "00101011", 9 => "11010100", 10 => "01011111", 11 => "00110100", 
    12 => "10110111", 13 => "00110111", 14 => "00000110", 15 => "01010110", 
    16 => "00111000", 17 => "10101110", 18 => "10101011", 19 => "01001000", 
    20 => "10011011", 21 => "00111100", 22 => "01011000", 23 => "11001000", 
    24 => "11010111", 25 => "11101010", 26 => "00010111", 27 => "10100100", 
    28 => "01010111", 29 => "11010010", 30 => "10100100", 31 => "10011011", 
    32 => "10111000", 33 => "10110100", 34 => "11001001", 35 => "10011010", 
    36 => "00100101", 37 => "00011001", 38 => "11100101", 39 => "00110100", 
    40 => "00010101", 41 => "00011000", 42 => "10000100", 43 => "11110101", 
    44 => "11000101", 45 => "00000001", 46 => "11010110", 47 => "10100111", 
    48 => "01101101", 49 => "00000000", 50 => "11101011", 51 => "00010110", 
    52 => "10111000", 53 => "11000101", 54 => "01010010", 55 => "10110001", 
    56 => "00001001", 57 => "11110001", 58 => "10111110", 59 => "11101000", 
    60 => "00101110", 61 => "10011000", 62 => "00011001", 63 => "10101010" );

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

entity dense_layer10_weisc4 is
    generic (
        DataWidth : INTEGER := 8;
        AddressRange : INTEGER := 64;
        AddressWidth : INTEGER := 6);
    port (
        reset : IN STD_LOGIC;
        clk : IN STD_LOGIC;
        address0 : IN STD_LOGIC_VECTOR(AddressWidth - 1 DOWNTO 0);
        ce0 : IN STD_LOGIC;
        q0 : OUT STD_LOGIC_VECTOR(DataWidth - 1 DOWNTO 0));
end entity;

architecture arch of dense_layer10_weisc4 is
    component dense_layer10_weisc4_rom is
        port (
            clk : IN STD_LOGIC;
            addr0 : IN STD_LOGIC_VECTOR;
            ce0 : IN STD_LOGIC;
            q0 : OUT STD_LOGIC_VECTOR);
    end component;



begin
    dense_layer10_weisc4_rom_U :  component dense_layer10_weisc4_rom
    port map (
        clk => clk,
        addr0 => address0,
        ce0 => ce0,
        q0 => q0);

end architecture;


