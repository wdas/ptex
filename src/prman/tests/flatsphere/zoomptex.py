import prman
prman.Init(["-progress", "-t:4"])
ri = prman.Ri()

def doFrame(framenum):
    ri.Begin(ri.RENDER)
    ri.ReadArchive("head.rib");
    ## Emit display and camera
    framename = "zoom/frame.%04d.tif" % framenum
#    ri.Display(framename, "tiff", "rgba")
    ri.Display(framename, "it", "rgba")
    ri.ConcatTransform([ 0.754709, -0.583187, -0.300511, -8.79864e-09,  -1.30048e-08, 0.458055, -0.888924, -9.08051e-11,  -0.656059, -0.67088, -0.345698, 7.58655e-09,  0.0209555, 0.129911, 2, 1 ])
    blur = 0.01 + framenum * 0.001
    ri.ReadArchive("mid.rib")

    ri.Surface("ptextest", {"string mapname":"pPyramid1.ptx",
                            "uniform float sharpness":0,
                            "uniform float blur":blur
                            })

    ri.ReadArchive("tail.rib")
    ri.End()
    print "Done with frame %d (blur=%g)\n" % (framenum, blur)


r = range(0, 300, 3)
#r = [299]
#r = [239, 240]
#r = [1000]
for framenum in r:
    doFrame(framenum)
