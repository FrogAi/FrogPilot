# otisserv - Copyright (c) 2019-, Rick Lan, dragonpilot community, and a number of other of contributors.
# Fleet Manager - [actuallylemoncurd](https://github.com/actuallylemoncurd), [AlexandreSato](https://github.com/alexandreSato), [ntegan1](https://github.com/ntegan1), [royjr](https://github.com/royjr), and [sunnyhaibin] (https://github.com/sunnypilot)
# Almost everything else - ChatGPT
# dirty PR pusher - mike8643
# Thepond author - Aidenir
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# This file contains all the functionality that I (Aidenir) hasn't updated from
# fleet_manager. Everything in here though I lifted straight out of that project.

from typing import List
import os
import subprocess

# If running on comma device
if os.path.exists("/data/persist"):
  from openpilot.common.swaglog import cloudlog
  from tools.lib.route import SegmentName
else:
  from .fixtures.fake_modules.route import SegmentName

def segment_to_segment_name(data_dir, segment) -> SegmentName:
  fake_dongle = "ffffffffffffffff"
  return SegmentName(str(os.path.join(data_dir, fake_dongle + "|" + segment)))

def get_directory_sort(d: str) -> List[str]:
  return [s.rjust(10, '0') for s in d.rsplit('--', 1)]

def get_all_segment_names(footage_path) -> list:
  segments = []
  for segment in listdir_by_creation(footage_path):
    try:
      segments.append(segment_to_segment_name(footage_path, segment))
    except AssertionError:
      pass
  return segments

def listdir_by_creation(d: str) -> List[str]:
  if not os.path.isdir(d):
    print("no such dir" + d)
    return []

  try:
    paths = [f for f in os.listdir(d) if os.path.isdir(os.path.join(d, f))]
    print(paths)
    paths = sorted(paths, key=get_directory_sort)
    return paths
  except OSError:
    cloudlog.exception("listdir_by_creation failed")
    return []
  
def get_segments_in_route(route, footage_path) -> list:
  segment_names = [segment_name for segment_name in get_all_segment_names(footage_path) if segment_name.time_str == route]
  segments = [segment_name.time_str + "--" + str(segment_name.segment_num) for segment_name in segment_names]
  return segments

def get_routes_names(footage_path) -> list:
  """
  Returns a list of all the routes in the footage directory,
  without the segment number.
  """
  segment_names = get_all_segment_names(footage_path)
  route_names = [segment_name.route_name for segment_name in segment_names]
  route_times = [route_name.time_str for route_name in route_names]
  unique_routes = list(dict.fromkeys(route_times))
  return sorted(unique_routes, reverse=True)

def list_file(path)-> list: 
  files = os.listdir(path)
  sorted_files = sorted(files, reverse=True)
  return sorted_files

def ffmpeg_mp4_wrap_process_builder(filename):
  """Returns a process that will wrap the given filename
     inside a mp4 container, for easier playback by browsers
     and other devices. Primary use case is streaming segment videos
     to the vidserver tool.
     filename is expected to be a pathname to one of the following
       /path/to/a/qcamera.ts
       /path/to/a/dcamera.hevc
       /path/to/a/ecamera.hevc
       /path/to/a/fcamera.hevc
  """
  basename = filename.rsplit("/")[-1]
  extension = basename.rsplit(".")[-1]
  command_line = ["ffmpeg"]
  if extension == "hevc":
    command_line += ["-f", "hevc"]
  command_line += ["-r", "20"]
  command_line += ["-i", filename]
  command_line += ["-c", "copy"]
  command_line += ["-map", "0"]
  if extension == "hevc":
    command_line += ["-vtag", "hvc1"]
  command_line += ["-f", "mp4"]
  command_line += ["-movflags", "empty_moov"]
  command_line += ["-"]
  return subprocess.Popen(
    command_line, stdout=subprocess.PIPE
  )