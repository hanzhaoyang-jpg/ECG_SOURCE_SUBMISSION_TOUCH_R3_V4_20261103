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

entity dense_layer10_weiudo_rom is 
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


architecture rtl of dense_layer10_weiudo_rom is 

signal addr0_tmp : std_logic_vector(AWIDTH-1 downto 0); 
type mem_array is array (0 to MEM_SIZE-1) of std_logic_vector (DWIDTH-1 downto 0); 
signal mem : mem_array := (
    0 => "10101001", 1 => "11010011", 2 => "10100000", 3 => "10111101", 
    4 => "11110110", 5 => "11000000", 6 => "00101110", 7 => "11010011", 
    8 => "00100100", 9 => "01100000", 10 => "11010110", 11 => "11111110", 
    12 => "11011010", 13 => "00001101", 14 => "11110100", 15 => "11000001", 
    16 => "11100000", 17 => "11110001", 18 => "10100101", 19 => "00100001", 
    20 => "10110110", 21 => "11010000", 22 => "10001111", 23 => "01110100", 
    24 => "11011101", 25 => "00100000", 26 => "11111001", 27 => "11100111", 
    28 => "10100000", 29 => "11100011", 30 => "01011001", 31 => "10110111", 
    32 => "00110011", 33 => "01001001", 34 => "11010100", 35 => "01101000", 
    36 => "11111001", 37 to 38=> "01010111", 39 => "10101101", 40 => "10011111", 
    41 => "11101000", 42 => "00111000", 43 => "00010000", 44 to 45=> "10110011", 
    46 => "10111010", 47 => "11001010", 48 => "11111000", 49 => "10100111", 
    50 => "10011111", 51 => "11000011", 52 => "11101001", 53 => "10011111", 
    54 => "10100011", 55 => "11000101", 56 => "00110000", 57 => "00000111", 
    58 => "01000000", 59 => "00010011", 60 => "00001011", 61 => "01011101", 
    62 => "01101111", 63 => "01010001" );

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

entity dense_layer10_weiudo is
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

architecture arch of dense_layer10_weiudo is
    component dense_layer10_weiudo_rom is
        port (
            clk : IN STD_LOGIC;
            addr0 : IN STD_LOGIC_VECTOR;
            ce0 : IN STD_LOGIC;
            q0 : OUT STD_LOGIC_VECTOR);
    end component;



begin
    dense_layer10_weiudo_rom_U :  component dense_layer10_weiudo_rom
    port map (
        clk => clk,
        addr0 => address0,
        ce0 => ce0,
        q0 => q0);

end architecture;


