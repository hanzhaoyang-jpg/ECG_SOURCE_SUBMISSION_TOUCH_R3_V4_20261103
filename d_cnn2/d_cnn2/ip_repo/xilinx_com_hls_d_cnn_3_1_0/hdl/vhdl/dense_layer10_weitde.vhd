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

entity dense_layer10_weitde_rom is 
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


architecture rtl of dense_layer10_weitde_rom is 

signal addr0_tmp : std_logic_vector(AWIDTH-1 downto 0); 
type mem_array is array (0 to MEM_SIZE-1) of std_logic_vector (DWIDTH-1 downto 0); 
signal mem : mem_array := (
    0 => "11001101", 1 => "10101001", 2 => "01101000", 3 => "10010111", 
    4 => "01011001", 5 => "11111111", 6 => "00110100", 7 => "00110111", 
    8 => "00100101", 9 => "11011010", 10 => "10111000", 11 => "11100110", 
    12 => "00100001", 13 => "11110110", 14 => "10110010", 15 => "00111101", 
    16 => "11111110", 17 => "10010110", 18 => "01001100", 19 => "00111101", 
    20 => "10010111", 21 => "00101110", 22 => "00100011", 23 => "11000100", 
    24 => "10001101", 25 => "10001100", 26 => "11111011", 27 => "00110000", 
    28 => "11100101", 29 => "00000000", 30 => "00101010", 31 => "11110111", 
    32 => "00000111", 33 => "00001010", 34 => "11111000", 35 => "10110011", 
    36 => "01100111", 37 => "10011101", 38 => "01011000", 39 => "01001100", 
    40 => "00000011", 41 => "11101011", 42 to 43=> "01001000", 44 => "11101111", 
    45 => "11110000", 46 => "10011000", 47 => "11000100", 48 => "10111110", 
    49 => "00000010", 50 => "00010111", 51 => "10000001", 52 => "11110111", 
    53 => "01100001", 54 => "11001011", 55 => "00000101", 56 => "01010101", 
    57 => "10101100", 58 => "01011110", 59 => "00000000", 60 => "10100010", 
    61 => "00101001", 62 => "00111100", 63 => "10110011" );

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

entity dense_layer10_weitde is
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

architecture arch of dense_layer10_weitde is
    component dense_layer10_weitde_rom is
        port (
            clk : IN STD_LOGIC;
            addr0 : IN STD_LOGIC_VECTOR;
            ce0 : IN STD_LOGIC;
            q0 : OUT STD_LOGIC_VECTOR);
    end component;



begin
    dense_layer10_weitde_rom_U :  component dense_layer10_weitde_rom
    port map (
        clk => clk,
        addr0 => address0,
        ce0 => ce0,
        q0 => q0);

end architecture;


