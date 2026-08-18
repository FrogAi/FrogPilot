#!/usr/bin/env python3
import gc
import cereal.messaging as messaging
from openpilot.common.realtime import Ratekeeper, set_realtime_priority

def dmonitoringd_thread():
    gc.disable()
    set_realtime_priority(2)

    pm = messaging.PubMaster(['driverMonitoringState'])
    rk = Ratekeeper(20, print_delay_threshold=None)

    while True:
        dat = messaging.new_message('driverMonitoringState')
        dat.valid = True

        dm = dat.driverMonitoringState
        dm.events = []
        dm.faceDetected = True
        dm.isDistracted = False
        dm.awarenessStatus = 1.0
        dm.posePitch = 0.0
        dm.poseYaw = 0.0
        dm.poseRoll = 0.0

        pm.send('driverMonitoringState', dat)
        rk.monitor_time()

def main():
    dmonitoringd_thread()

if __name__ == '__main__':
    main()
