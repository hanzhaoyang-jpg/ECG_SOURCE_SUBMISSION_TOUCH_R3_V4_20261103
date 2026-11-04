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

entity dense_layer10_weircU_rom is 
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


architecture rtl of dense_layer10_weircU_rom is 

signal addr0_tmp : std_logic_vector(AWIDTH-1 downto 0); 
type mem_array is array (0 to MEM_SIZE-1) of std_logic_vector (DWIDTH-1 downto 0); 
signal mem : mem_array := (
    0 => "01100001", 1 => "00101010", 2 => "10111010", 3 => "11000001", 
    4 => "00100000", 5 => "00100001", 6 => "01010111", 7 => "00111000", 
    8 => "01010000", 9 => "01001101", 10 => "11110100", 11 => "00001010", 
    12 => "00010111", 13 => "11100111", 14 => "01010101", 15 => "01100001", 
    16 => "11101000", 17 => "10101101", 18 => "01010111", 19 => "11000010", 
    20 => "01100000", 21 => "11010010", 22 => "11110011", 23 => "00101111", 
    24 => "11011011", 25 => "00100111", 26 => "10001111", 27 => "01010110", 
    28 => "11010111", 29 => "01000110", 30 => "00010001", 31 => "10111110", 
    32 => "00011111", 33 => "10110011", 34 => "11111000", 35 => "00000111", 
    36 => "11101010", 37 => "00101110", 38 => "01000101", 39 => "10111110", 
    40 => "00100001", 41 to 42=> "10111010", 43 => "10111000", 44 => "01010000", 
    45 => "00111100", 46 => "01001001", 47 => "01011000", 48 => "10010011", 
    49 => "11010100", 50 => "11101010", 51 => "01010000", 52 => "11000011", 
    53 => "00101000", 54 => "00111010", 55 => "01011010", 56 => "00100101", 
    57 => "00101010", 58 => "00011100", 59 => "00011101", 60 => "11110010", 
    61 => "11010110", 62 => "11010000", 63 => "10101110" );

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

entity dense_layer10_weircU is
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

architecture arch of dense_layer10_weircU is
    component dense_layer10_weircU_rom is
        port (
            clk : IN STD_LOGIC;
            addr0 : IN STD_LOGIC_VECTOR;
            ce0 : IN STD_LOGIC;
            q0 : OUT STD_LOGIC_VECTOR);
    end component;



begin
    dense_layer10_weircU_rom_U :  component dense_layer10_weircU_rom
    port map (
        clk => clk,
        addr0 => address0,
        ce0 => ce0,
        q0 => q0);

end architecture;


