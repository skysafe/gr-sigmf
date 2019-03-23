from datetime import datetime

from gnuradio import gr_unittest
from gr_sigmf import gr_sigmf_swig as sigmf


example_gprmc = \
    "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"
example_gpgga = \
    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47"
example_datetime = datetime(1994, 3, 23, 12, 35, 19)

example_gprmc_missing_start = \
    "GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A"
example_gprmc_missing_end = \
    "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W"
example_gprmc_bad_checksum = \
    "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*72"


class qa_nmea_parser(gr_unittest.TestCase):

    def test_extract_good(self):
        payload = sigmf.nmea_extract(example_gprmc)
        self.assertTrue(payload.startswith("GPRMC"))

    def test_extract_missing_start(self):
        with self.assertRaises(RuntimeError):
            sigmf.nmea_extract(example_gprmc_missing_start)

    def test_extract_missing_end(self):
        with self.assertRaises(RuntimeError):
            sigmf.nmea_extract(example_gprmc_missing_end)

    def test_extract_bad_checksum(self):
        with self.assertRaises(RuntimeError):
            sigmf.nmea_extract(example_gprmc_bad_checksum)

    def test_parse_gprmc(self):
        msg = sigmf.gprmc_message.parse(example_gprmc)
        self.assertAlmostEqual(msg.lat, 48.1173)
        self.assertAlmostEqual(msg.lon, 11.51666667)
        self.assertTrue(msg.active)
        self.assertAlmostEqual(msg.speed_knots, 22.4)
        self.assertAlmostEqual(msg.track_angle, 84.4)
        self.assertAlmostEqual(msg.magnetic_variation, -3.1)

        dt = datetime.utcfromtimestamp(msg.timestamp)
        self.assertEqual(dt, example_datetime)

        self.assertEqual(msg.date, "230394")
        self.assertEqual(msg.time, "123519")

    def test_parse_gpgga(self):
        msg = sigmf.gpgga_message.parse(example_gpgga)
        self.assertAlmostEqual(msg.lat, 48.1173)
        self.assertAlmostEqual(msg.lon, 11.51666667)
        self.assertEqual(msg.fix_quality, 1)
        self.assertEqual(msg.num_sats, 8)
        self.assertAlmostEqual(msg.hdop, 0.9)
        self.assertAlmostEqual(msg.altitude_msl, 545.4)
        self.assertAlmostEqual(msg.geoid_hae, 46.9)

        self.assertEqual(msg.time, "123519")


if __name__ == '__main__':
    gr_unittest.run(qa_nmea_parser, "qa_nmea_parser.xml")
