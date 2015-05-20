import socket
import time
import array
import select
import errno

import threading

import uavtalk, uavo_collection, uavo

import os

from abc import ABCMeta, abstractmethod

class TelemetryBase():
    """
    Provides a basic telemetry connection to a flight controller
    """

    # This is not a complete implemention / must subclass
    __metaclass__ = ABCMeta

    def __init__(self, uavo_defs=None, githash=None, service_in_iter=True,
            iter_blocks=True, use_walltime=True, do_handshaking=False,
            gcs_timestamps=False):
        if uavo_defs is None:
            uavo_defs = uavo_collection.UAVOCollection()

            if githash:
                uavo_defs.from_git_hash(githash)
            else:
                uavo_defs.from_uavo_xml_path("shared/uavobjectdefinition")

        self.githash = githash

        self.uavo_defs = uavo_defs
        self.uavtalk_generator = uavtalk.process_stream(uavo_defs,
            use_walltime=use_walltime, gcs_timestamps=gcs_timestamps)

        self.uavtalk_generator.send(None)

        self.uavo_list = []

        self.last_values = {}

        self.cond = threading.Condition()

        self.service_in_iter = service_in_iter
        self.iter_blocks = iter_blocks

        self.do_handshaking = do_handshaking

    def as_numpy_array(self, match_class): 
        import numpy as np

        # Find the subset of this list that is of the requested class 
        filtered_list = filter(lambda x: isinstance(x, match_class), self) 
 
        # Check for an empty list 
        if filtered_list == []: 
            return np.array([]) 
 
        # Find the uavo definition associated with this UAVO type 
        if not "{0:08x}".format(filtered_list[0].uavo_id) in self.uavo_defs: 
            dtype = None 
        else: 
            uavo_def = self.uavo_defs["{0:08x}".format(filtered_list[0].uavo_id)] 
            dtype  = [('name', 'S20'), ('time', 'double'), ('uavo_id', 'uint')] 
 
            for f in uavo_def.fields: 
                dtype += [(f['name'], '(' + `f['elements']` + ",)" + uavo_def.type_numpy_map[f['type']])] 
 
        return np.array(filtered_list, dtype=dtype) 

    def __iter__(self):
        iterIdx = 0

        self.cond.acquire()

        with self.cond:
            while True:
                if iterIdx < len(self.uavo_list):
                    obj = self.uavo_list[iterIdx]

                    iterIdx += 1

                    self.cond.release()
                    try:
                        yield obj
                    finally:
                        self.cond.acquire()
                elif self.iter_blocks and not self._done():
                    if self.service_in_iter:
                        self.cond.release()

                        try:
                            self.service_connection()
                        finally:
                            self.cond.acquire()
                    else:
                        # wait for another thread to fill it in
                        self.cond.wait()
                else:
                    # Don't really recommend this mode anymore/maybe remove
                    if self.service_in_iter and not self._done():
                        # Do at least one non-blocking attempt
                        self.cond.release()

                        try:
                            self.service_connection(0)
                        finally:
                            self.cond.acquire()

                    if iterIdx >= len(self.uavo_list):
                        break

    def __make_handshake(self, handshake):
        return uavo.UAVO_GCSTelemetryStats._make_to_send(0, 0, 0, 0, 0, handshake)

    def __handle_handshake(self, obj):
        if obj.name == "FlightTelemetryStats":
            # Handle the telemetry handshaking

            (DISCONNECTED, HANDSHAKE_REQ, HANDSHAKE_ACK, CONNECTED) = (0,1,2,3)

            if obj.Status == DISCONNECTED:
                # Request handshake
                print "Disconnected"
                send_obj = self.__make_handshake(HANDSHAKE_REQ)
            elif obj.Status == HANDSHAKE_ACK:
                # Say connected
                print "Handshake ackd"
                send_obj = self.__make_handshake(CONNECTED)
            elif obj.Status == CONNECTED:
                print "Connected"
                send_obj = self.__make_handshake(CONNECTED)

            self._send(uavtalk.send_object(send_obj))

    def __handleFrames(self, frames):
        objs = []

        obj = self.uavtalk_generator.send(frames)

        while obj:
            if self.do_handshaking:
                self.__handle_handshake(obj)

            objs.append(obj)

            obj = self.uavtalk_generator.send('')

        # Only traverse the lock when we've processed everything in this
        # batch.
        with self.cond:
            # keep everything in ram forever
            # for now-- in case we wanna see
            self.uavo_list.extend(objs)

            for obj in objs:
                self.last_values[obj.name]=obj

            self.cond.notifyAll()

    def get_last_values(self):
        with self.cond:
            return self.last_values.copy()

    def start_thread(self):
        if self.service_in_iter:
            raise

        if self._done():
            raise

        from threading import Thread

        def run():
            while not self._done():
                self.service_connection()    

        t = Thread(target=run, name="telemetry svc thread")

        t.daemon=True

        t.start()

    def service_connection(self, timeout=None):
        """
        Receive and parse data from a connection and handle the basic
        handshaking with the flight controller
        """

        if timeout is not None:
            finish_time = time.time()+timeout
        else:
            finish_time = None

        data = self._receive(finish_time)
        self.__handleFrames(data)

    @abstractmethod
    def _receive(self, finish_time):
        return

    # No implementation required, so not abstract
    def _send(self, msg):
        return

    @abstractmethod
    def _done(self):
        return True

class FDTelemetry(TelemetryBase):
    # expects fd is set nonblocking
    # intended for bidirectional comms.
    def __init__(self, fd, *args, **kwargs):
        TelemetryBase.__init__(self, do_handshaking=True,
                gcs_timestamps=False,  *args, **kwargs)

        self.recv_buf = ''
        self.send_buf = ''

        self.fd = fd

    def _receive(self, finish_time):
        """ Fetch available data from TCP socket """

        # Always do some minimal IO if possible
        self._do_io(0)

        while (len(self.recv_buf) < 1) and self._do_io(finish_time):
            pass

        if len(self.recv_buf) < 1:
            return None

        ret = self.recv_buf
        self.recv_buf = ''

        return ret

    # Call select and do one set of IO operations.
    def _do_io(self, finish_time):
        rdSet = []
        wrSet = []

        didStuff = False

        if len(self.recv_buf) < 1024:
            rdSet.append(self.fd)

        if len(self.send_buf) > 0:
            wrSet.append(self.fd)

        now = time.time()
        if finish_time is None: 
            r,w,e = select.select(rdSet, wrSet, [])
        else:
            tm = finish_time-now
            if tm < 0: tm=0

            r,w,e = select.select(rdSet, wrSet, [], tm)

        if r:
            # Shouldn't throw an exception-- they just told us
            # it was ready for read.
            chunk = os.read(self.fd, 1024)
            if chunk == '':
                raise RuntimeError("stream closed")

            self.recv_buf = self.recv_buf + chunk

            didStuff = True

        if w:
            written = os.write(self.fd, self.send_buf)

            if written > 0:
                self.send_buf = self.send_buf[written:]

            didStuff = True

        return didStuff

    def _send(self, msg):
        """ Send a string out the TCP socket """

        self.send_buf += msg

        self._do_io(0)

    def _done(self):
        return self.fd is None

class NetworkTelemetry(FDTelemetry):
    def __init__(self, host="127.0.0.1", port=9000, *args, **kwargs):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((host, port))

        s.setblocking(0)

        self.sock = s

        FDTelemetry.__init__(self, fd=s.fileno(), *args, **kwargs)

class FileTelemetry(TelemetryBase):
    # TODO accept file object
    def __init__(self, filename='sim_log.tll', parse_header=False,
             *args, **kwargs):
        self.filename = filename
        self.f = file(filename, 'r')

        if parse_header:
            # Check the header signature
            #    First line is "Tau Labs git hash:"
            #    Second line is the actual git hash
            #    Third line is the UAVO hash
            #    Fourth line is "##"
            sig = self.f.readline()
            if sig != 'Tau Labs git hash:\n':
                print "Source file does not have a recognized header signature"
                print '|' + sig + '|'
                raise IOError("no header signature")
            # Determine the git hash that this log file is based on
            githash = self.f.readline()[:-1]
            if githash.find(':') != -1:
                import re
                githash = re.search(':(\w*)\W', githash).group(1)

            print "Log file is based on git hash: %s" % githash

            uavohash = self.f.readline()
            divider = self.f.readline()

            TelemetryBase.__init__(self, service_in_iter=False, iter_blocks=True,
                do_handshaking=False, githash=githash, *args, **kwargs)
        else:
            TelemetryBase.__init__(self, service_in_iter=False, iter_blocks=True,
                do_handshaking=False, *args, **kwargs)

        self.done=False
        self.start_thread()

    def _receive(self, finish_time):
        """ Fetch available data from file """

        buf = self.f.read(128)

        if buf == '':
            self.done=True

        return buf

    def _done(self):
        return self.done

def _normalize_path(path):
    import os
    return os.path.normpath(os.path.join(os.getcwd(), path))

def get_telemetry_by_args(desc="Process telemetry"):
    # Setup the command line arguments.
    # XXX DESC, ETC
    import argparse
    parser = argparse.ArgumentParser(description=desc)

    # Log format indicates this log is using the old file format which
    # embeds the timestamping information between the UAVTalk packet 
    # instead of as part of the packet
    parser.add_argument("-t", "--timestamped",
                        action  = 'store_false',
                        default = True,
                        help    = "indicate that this is an overo log file or some format that has timestamps")

    parser.add_argument("-g", "--githash",
                        action  = "store",
                        dest    = "githash",
                        help    = "override githash for UAVO XML definitions")

    parser.add_argument("source",
                        help  = "log file for processing")

    # Parse the command-line.
    args = parser.parse_args()

    # Open the log file
    src = _normalize_path(args.source)

    parse_header = False
    githash = None

    if args.githash is not None:
        # If we specify the log header no need to attempt to parse it
        githash = args.githash
    else:
        parse_header = True

    from taulabs import telemetry

    return telemetry.FileTelemetry(filename=src, parse_header=parse_header,
        gcs_timestamps=args.timestamped)

