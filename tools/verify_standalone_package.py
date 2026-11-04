"""Read-only integrity and address checks, not a board-boot claim."""
from pathlib import Path
import hashlib
import configparser
import json
import struct
import zipfile
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else ROOT / 'build_output/standalone_20261028'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def hardware_map(archive):
    name, = (n for n in archive.namelist() if n.endswith('.hwh'))
    root = ET.fromstring(archive.read(name))
    ranges = {tuple(sorted(item.attrib.items())) for item in root.iter('MEMRANGE')}
    widths = {item.get('VALUE') for item in root.iter('PARAMETER')
              if item.get('NAME') in ('C_EMIO_GPIO_WIDTH', 'PCW_GPIO_EMIO_GPIO_WIDTH')}
    return ranges, widths


def loads(path):
    data = path.read_bytes()
    assert data[:7] == b'\x7fELF\x01\x01\x01'
    assert struct.unpack_from('<H', data, 18)[0] == 40  # EM_ARM
    entry, start = struct.unpack_from('<II', data, 24)
    entry_size, count = struct.unpack_from('<HH', data, 42)
    result = []
    for i in range(count):
        kind, offset, va, pa, file_size, memory_size, flags, align = struct.unpack_from(
            '<8I', data, start + i * entry_size)
        if kind == 1:
            assert offset + file_size <= len(data)
            result.append({'address': pa, 'end': pa + memory_size, 'file_bytes': file_size,
                           'memory_bytes': memory_size, 'flags': flags, 'alignment': align})
    return entry, result


def main():
    manifest = json.loads((OUT / 'build_manifest.json').read_text(encoding='utf-8'))
    for name, expected in manifest['hashes'].items():
        assert sha(Path(name).read_bytes()) == expected, name
    config = configparser.ConfigParser(interpolation=None, comment_prefixes=('#', ';'))
    config.read_string('[profile]\n' + (OUT / 'sd_card/ECGCFG.INI').read_text(encoding='utf-8-sig'))
    profile = {key: int(value) for key, value in config['profile'].items()}
    assert profile.pop('version') == 1
    assert profile == manifest['default'], 'Delivery configuration differs from manifest'
    hdf = Path(manifest.get('hardware_hdf', ROOT / 'build_output/status_pins_20261008/design_1_wrapper.hdf'))
    fsbl = Path(manifest.get('fsbl_elf', OUT / 'fsbl_workspace/ecg_fsbl/Debug/ecg_fsbl.elf'))
    with zipfile.ZipFile(hdf) as archive:
        names = [n for n in archive.namelist() if n.endswith('.bit')]
        assert len(names) == 1
        assert sha(archive.read(names[0])) == sha(hdf.with_suffix('.bit').read_bytes())
        tcl = [n for n in archive.namelist() if n.endswith('ps7_init.tcl')]
        assert len(tcl) == 1
        assert sha(archive.read(tcl[0])) == sha((fsbl.parents[2] / 'ecg_hw/ps7_init.tcl').read_bytes())
        if manifest.get('profile') in ('offline', 'workstation'):
            ranges, widths = hardware_map(archive)
            assert widths == {'6'}, 'Touch HDF does not have six EMIO GPIOs'
            with zipfile.ZipFile(ROOT / 'build_output/status_pins_20261008/design_1_wrapper.hdf') as original:
                previous, _ = hardware_map(original)
            assert ranges and ranges == previous, 'Old BSP address map is not compatible'
    entry, app_loads = loads(OUT / 'SD_state.elf')
    assert entry == 0x100000
    for segment in app_loads:
        assert 0x100000 <= segment['address'] < segment['end'] < 0x10000000
    _, fsbl_loads = loads(fsbl)
    for segment in fsbl_loads:
        assert (segment['address'] < 0x30000 and segment['end'] <= 0x30000) or (
            segment['address'] >= 0xFFFF0000 and segment['end'] <= 0xFFFFFF00)
    image = (OUT / 'sd_card/BOOT.BIN').read_bytes()
    assert struct.unpack_from('<I', image, 0x20)[0] == 0xAA995566
    assert struct.unpack_from('<I', image, 0x24)[0] == 0x584C4E58
    assert manifest['firmware'].encode('ascii') in image
    assert manifest['firmware'].encode('ascii') in (OUT / 'SD_state.elf').read_bytes()
    assert len(image) > 4_000_000
    result = {'state': 'offline_package_integrity_checked_not_board_boot_tested',
        'hashes_checked': len(manifest['hashes']), 'hdf_bit_and_ps_init_match': True,
        'app_entry': entry, 'app_load_segments': app_loads,
        'framebuffer_base': 0x10000000, 'fsbl_load_segments': fsbl_loads,
        'boot_bytes': len(image), 'firmware_revision_in_boot_image': True,
        'delivery_profile_matches_manifest': True, 'profile': profile,
        'touch_emio_width': 6 if manifest.get('profile') in ('offline', 'workstation') else None,
        'physical_board_reset_or_programmed': False}
    (OUT / 'package_audit.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
