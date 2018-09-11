import glob
import os
import subprocess
import multiprocessing
import time
import jinja2

templateLoader = jinja2.FileSystemLoader(searchpath="./")
templateEnv = jinja2.Environment(loader=templateLoader)
TEMPLATE_FILE = "template.xml"
template = templateEnv.get_template(TEMPLATE_FILE)

videos = glob.glob("*.[mM][oO][vV]")
videos.extend(glob.glob("*.[mM][pP]4"))
videos.extend(glob.glob("*.[mM][tT][sS]"))
work1 = []
work2 = []
work3 = []
work4 = []
count = 0

for item in videos:
    switcher = {
        0: lambda x: work1.append(x),
        1: lambda x: work2.append(x),
        2: lambda x: work3.append(x),
        3: lambda x: work4.append(x),
    }
    switcher[count](item)
    count += 1
    if count == 4:
        count = 0
processes = []


def running(item):
    print(item)
    j = ['./hipsort']
    j.extend(item)
    subprocess.Popen(j).wait()
    return ""


pool = multiprocessing.Pool(processes=4)
now = time.time()
pool.map(running, [work1, work2, work3, work4])
print(time.time() - now)
txts = glob.glob("result/*.txt")
lists = []
for item in txts:
    sp_name = item.split(".")
    if "/" in item:
        sp_name = item.split("/")[-1].split(".")

    name = sp_name[0] + "." + sp_name[1]
    f = open(item, "r")
    line = f.readline()
    times = line.split(" ")
    lists.append({
        "name": name,
        "range": int(times[0]),
        "start": int(times[1]),
        "end": int(times[2]),
    })
    f.close()

lists = sorted(lists, key=lambda x: x["start"])
start = lists[0]["start"]

results = []

# < !-- name -->
# < !-- start -->
# < !-- end -->
# < !-- path -->
# < !-- width -->
# < !-- height -->
ftemplate = []
# https://developer.apple.com/library/archive/documentation/AppleApplications/Reference/FinalCutPro_XML/Basics/Basics.html#//apple_ref/doc/uid/TP30001154-BAJGHFBA
fidx = 0
for item in lists:
    diff = item["start"] - start
    hour = int(diff / 3600000)
    diff -= hour * 3600000
    minutes = int(diff / 60000)
    diff -= minutes * 60000
    seconds = int(diff / 1000)
    diff -= seconds * 1000
    ms = int(diff / 10)
    # results.append((item["name"] hour, minutes, seconds, ms)
    time_code = "{0}:{1}:{2};{3}".format(str(hour).zfill(2),
                                         str(minutes).zfill(2),
                                         str(seconds).zfill(2),
                                         str(ms).zfill(2))
    results.append({"timecode": time_code, "name": item["name"]})

    duration = int(item["range"] / 1000 * 30)
    ss = int((item["start"] - start) * 30 / 1000)
    ftemplate.append({
        "index": fidx,
        "name": item["name"],
        "duration": duration,
        "start": ss,
        "end": ss + duration,
        "path": os.path.abspath(item["name"]),
        "width": 1920,
        "height": 1080,
    })
    fidx += 1
    print("{0} : {1}".format(item["name"], time_code))

def range_check(arr):
    first = arr[0]
    a = []
    b = []
    for i in range(1, len(arr)):
        if first["start"] <= arr[i]["start"] <= first["end"]:
            a.append(arr[i])
        else:
            first = arr[i]
            b.append(arr[i])
    return [a, b]


tracks = [[]]
start_list = ftemplate
for item in ftemplate:
    inserted = False

    for idx, track in enumerate(tracks):
        if len(track) == 0:
            tracks[idx].append(item)
            inserted = True
            break

        for clipIdx in range(1, len(track)):
            left = track[clipIdx - 1]["end"]
            right = track[clipIdx]["start"]
            if left > item["start"] and item["end"] < right:
                tracks[idx].insert(clipIdx, item)
                inserted = True
                break

        if not inserted:
            if track[-1]["end"] < item["start"]:
                tracks[idx].append(item)
                inserted = True

    if not inserted:
        tracks.append([item])

outputText = template.render({"tracks": tracks})
file = open("project.xml", "w")
file.write(outputText)

file.close()
