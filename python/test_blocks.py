from gnuradio import gr
from threading import Timer
import pmt
import numpy


class simple_tag_injector(gr.sync_block):
    def __init__(self):
        gr.sync_block.__init__(
            self,
            name="simple_tag_injector",
            in_sig=[numpy.complex64],
            out_sig=[numpy.complex64],
        )
        self.inject_tag = None
        self.injected_offset = None

    def work(self, input_items, output_items):
        output_items[0][:] = input_items[0]
        if self.inject_tag:
            offset = self.nitems_read(0)
            for key, val in self.inject_tag.items():
                self.add_item_tag(
                    0, offset, pmt.to_pmt(key), pmt.to_pmt(val))
            self.injected_offset = offset
            self.inject_tag = None
        return len(output_items[0])


class advanced_tag_injector(gr.sync_block):
    '''Like simple_tag_injector, but can specify exact
    indices to inject the tags at'''
    def __init__(self, tags_to_inject):
        gr.sync_block.__init__(
            self,
            name="advanced_tag_injector",
            in_sig=[numpy.complex64],
            out_sig=[numpy.complex64],
        )
        self.tags_to_inject = tags_to_inject

    def work(self, input_items, output_items):
        output_items[0][:] = input_items[0]
        start_of_range = self.nitems_read(0)
        end_of_range = start_of_range + len(input_items[0])
        for index, tags in self.tags_to_inject:
            if index >= start_of_range and index < end_of_range:
                print(tags)
                for key, val in tags.items():
                    self.add_item_tag(
                        0, index, pmt.to_pmt(key), pmt.to_pmt(val))
        print(start_of_range)
        return len(output_items[0])


class sample_counter(gr.sync_block):
    def __init__(self):
        gr.sync_block.__init__(
            self,
            name="sample_counter",
            in_sig=[numpy.complex64],
            out_sig=[numpy.complex64],
        )
        self.count = 0

    def work(self, input_items, output_items):
        output_items[0][:] = input_items[0]
        self.count += len(output_items[0])
        return len(output_items[0])


class msg_sender(gr.sync_block):
    def __init__(self):
        gr.sync_block.__init__(
            self,
            name="msg_sender",
            in_sig=None,
            out_sig=None
        )
        self.message_port_register_out(pmt.intern("out"))

    def send_msg(self, msg_to_send):
        self.message_port_pub(pmt.intern("out"), pmt.to_pmt(msg_to_send))


class sample_producer(gr.sync_block):
    def __init__(self, limit, limit_ev, continue_ev):
        gr.sync_block.__init__(
            self,
            name="sample_producer",
            in_sig=None,
            out_sig=[numpy.complex64],
        )
        self.limit = limit
        self.limit_ev = limit_ev
        self.continue_ev = continue_ev
        self.fired = False

    def work(self, input_items, output_items):
        if self.limit <= 0:
            if not self.fired:
                self.limit_ev.set()
                self.continue_ev.wait()
                self.fired = True
            samples_to_produce = len(output_items[0])
            for i in range(samples_to_produce):
                output_items[0][i] = 1 + 1j
            return samples_to_produce

        if self.limit > 0:
            samples_to_produce = int(min(self.limit, len(output_items[0])))
            for i in range(samples_to_produce):
                output_items[0][i] = 1 + 1j
            self.limit -= samples_to_produce
            return samples_to_produce


class message_generator(gr.basic_block):
    def __init__(self, message):
        gr.basic_block.__init__(self, name="message_generator",
                                in_sig=None, out_sig=None)
        self.message = message
        self.message_port_register_out(pmt.intern('messages'))
        self.timer = None

    def send_msg(self):
        msg = self.message
        if not isinstance(msg, pmt.pmt_swig.swig_int_ptr):
            msg = pmt.to_pmt(msg)
        self.message_port_pub(pmt.intern('messages'), msg)

    def stop(self):
        if self.timer:
            self.timer.cancel()
            self.timer.join()
        return True

    def start(self):
        self.timer = Timer(1, self.send_msg)
        self.timer.start()
        return True


# Block that just collects all tags that pass thorough it
class tag_collector(gr.sync_block):
    def __init__(self):
        gr.sync_block.__init__(
            self,
            name="tag_collector",
            in_sig=[numpy.complex64],
            out_sig=[numpy.complex64],
        )
        self.tags = []

    def work(self, input_items, output_items):
        tags = self.get_tags_in_window(0, 0, len(input_items[0]))
        if (tags):
            for tag in tags:
                self.tags.append({
                    "key": pmt.to_python(tag.key),
                    "offset": tag.offset,
                    "value": pmt.to_python(tag.value)
                })
        output_items[0][:] = input_items[0]
        return len(output_items[0])

    def assertTagExists(self, offset, key, val):
        assert len([t for t in self.tags if t["offset"] ==
                    offset and t["key"] == key and t["value"] == val]) == 1

    def assertTagExistsMsg(self, offset, key, val, msg, unit_test):
        unit_test.assertEqual(
            len([t for t in self.tags if t["offset"] ==
                 offset and t["key"] == key and t["value"] == val]), 1, msg)
