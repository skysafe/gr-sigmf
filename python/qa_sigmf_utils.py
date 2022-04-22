import sys
import os

from gnuradio import gr_unittest

try:
    import gr_sigmf as sigmf
except ImportError:
    dirname, filename = os.path.split(os.path.abspath(__file__))
    sys.path.append(os.path.join(dirname, "bindings"))
    import gr_sigmf as sigmf


class qa_sigmf_utils(gr_unittest.TestCase):

    def test_parse_format_str(self):
        for domain in ["r", "c"]:
            for type_str in ["f32", "f64", "i32", "i16", "u32", "u16"]:
                for endian in ["_le", "_be"]:
                    format_str = f"{domain}{type_str}{endian}"
                    parsed = sigmf.parse_format_str(format_str)
                    if domain == "r":
                        assert not parsed.is_complex, "bad parse"
                    else:
                        assert parsed.is_complex, "bad parse"
                    assert parsed.type_str == type_str, "bad parse"
                    assert parsed.width == int(type_str[1:]), "bad parse"
                    if endian == "_le":
                        assert parsed.endianness == sigmf.endian_t.LITTLE
                    else:
                        assert parsed.endianness == sigmf.endian_t.BIG
                    assert parsed.sample_size == (int(type_str[1:]) / 8 ) * (1 if domain == "r" else 2)
        
        for domain in ["r", "c"]:
            for type_str in ["i8", "u8"]:
                format_str = f"{domain}{type_str}"
                parsed = sigmf.parse_format_str(format_str)
                if domain == "r":
                    assert not parsed.is_complex, "bad parse"
                else:
                    assert parsed.is_complex, "bad parse"
                assert parsed.type_str == type_str, "bad parse"
                assert parsed.width == int(type_str[1:]), "bad parse"
                assert parsed.sample_size == (int(type_str[1:]) / 8 ) * (1 if domain == "r" else 2)


if __name__ == '__main__':
    gr_unittest.run(qa_sigmf_utils)
