from datetime import datetime

from gnuradio import gr_unittest
from gr_sigmf import gr_sigmf_swig as sigmf


# Example NMEA sentences from reference docs
good_gprmc = \
    "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"
good_gpgga = \
    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"
good_datetime = datetime(1994, 3, 23, 12, 35, 19)

# Sentences emitted by Jackson Labs LC_XO with no fix
good_gprmc_nofix = \
    "$GPRMC,015534.00,V,0000.0000,N,00000.0000,E,0.0,0.0,060106,,*23"
good_gpgga_nofix = \
    "$GPGGA,015534.00,0000.0000,N,00000.0000,E,0,99,1.0,0.0,M,0.0,M,,*5A"

# Sentences emitted by u-Blox LEA-M8 with no fix
good_gprmc_empty = "$GPRMC,,V,,,,,,,,,*31"
good_gpgga_empty = "$GPGGA,,,,,,0,00,99.99,,,,,,*48"

# Intentionally malformed sentences
bad_gprmc_missing_start = \
    "GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"
bad_gprmc_missing_end = \
    "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W"
bad_gprmc_bad_checksum = \
    "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*72"


class qa_nmea_parser(gr_unittest.TestCase):

    def test_extract_good(self):
        payload = sigmf.nmea_extract(good_gprmc)
        self.assertTrue(payload.startswith("GPRMC"))

    def test_extract_missing_start(self):
        with self.assertRaises(RuntimeError):
            sigmf.nmea_extract(bad_gprmc_missing_start)

    def test_extract_missing_end(self):
        with self.assertRaises(RuntimeError):
            sigmf.nmea_extract(bad_gprmc_missing_end)

    def test_extract_bad_checksum(self):
        with self.assertRaises(RuntimeError):
            sigmf.nmea_extract(bad_gprmc_bad_checksum)

    def test_parse_gprmc(self):
        msg = sigmf.gprmc_message.parse(good_gprmc)
        self.assertAlmostEqual(msg.lat, 48.1173)
        self.assertAlmostEqual(msg.lon, 11.51666667)
        self.assertTrue(msg.valid)
        self.assertAlmostEqual(msg.speed_knots, 22.4)
        self.assertAlmostEqual(msg.track_angle, 84.4)
        self.assertAlmostEqual(msg.magnetic_variation, -3.1)

        dt = datetime.utcfromtimestamp(msg.timestamp)
        self.assertEqual(dt, good_datetime)

        self.assertEqual(msg.date, "230394")
        self.assertEqual(msg.time, "123519")

    def test_parse_gprmc_nofix(self):
        msg = sigmf.gprmc_message.parse(good_gprmc_nofix)
        self.assertFalse(msg.valid)

    def test_parse_gprmc_empty(self):
        msg = sigmf.gprmc_message.parse(good_gprmc_empty)
        self.assertFalse(msg.valid)

    def test_parse_gpgga(self):
        msg = sigmf.gpgga_message.parse(good_gpgga)
        self.assertAlmostEqual(msg.lat, 48.1173)
        self.assertAlmostEqual(msg.lon, 11.51666667)
        self.assertEqual(msg.fix_quality, 1)
        self.assertEqual(msg.num_sats, 8)
        self.assertAlmostEqual(msg.hdop, 0.9)
        self.assertAlmostEqual(msg.altitude_msl, 545.4)
        self.assertAlmostEqual(msg.geoid_hae, 46.9)

        self.assertEqual(msg.time, "123519")

    def test_parse_gpgga_nofix(self):
        msg = sigmf.gpgga_message.parse(good_gpgga_nofix)
        self.assertEqual(msg.fix_quality, 0)

    def test_parse_gpgga_empty(self):
        msg = sigmf.gpgga_message.parse(good_gpgga_empty)
        self.assertEqual(msg.fix_quality, 0)


if __name__ == '__main__':
    gr_unittest.run(qa_nmea_parser, "qa_nmea_parser.xml")
