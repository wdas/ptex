import prman

def test(frame, lerp):
    filename = ri.RENDER
    ri.Begin(filename)
    ri.ReadArchive("head.rib");
    name = lerp and 'lerp' or 'nolerp'
    z = framenum * .0075
    framename = "frames/%s_%0df.tif" % (name, frame)
#    ri.Display(framename, "tiff", "rgba")
    ri.Display(framename, "it", "rgba")
    ri.ReadArchive("mid.rib")

    ri.Surface("test", {"string mapname": "rand/pPlane1.ptx",
                        "float dispScale": 0.015,
                        "float lerp": lerp})
    P = [-0.5, -0.5, 0, -0.16666666, -0.5, 0, -0.16666666, -0.16666666, z, -0.5, -0.16666666, 0,
         0.16666669, -0.5, 0, 0.16666669, -0.16666666, z, 0.5, -0.5, 0, 0.5, -0.16666666, 0,
         -0.16666666, 0.16666669, z, -0.5, 0.16666669, 0, 0.16666669, 0.16666669, z, 0.5, 0.16666669, 0,
         -0.16666666, 0.5, 0, -0.5, 0.5, 0, 0.16666669, 0.5, 0, 0.5, 0.5, 0]

    ri.SubdivisionMesh("catmull-clark", [4, 4, 4, 4, 4, 4, 4, 4, 4],
                       [3, 2, 1, 0,    2, 5, 4, 1,     5, 7, 6, 4,
                        9, 8, 2, 3,    8, 10, 5, 2,    10, 11, 7, 5,
                        13, 12, 8, 9,  12, 14, 10, 8,  14, 15, 11, 10],
                       ["crease", "crease", "crease", "crease", "crease", "crease", "crease",
                        "crease", "crease", "crease", "crease", "crease", "facevaryinginterpolateboundary"],
                       [2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 1, 0],
                       [15, 14, 14, 12, 12, 13, 11, 15, 13, 9, 7, 11, 9, 3, 6, 7, 4, 6, 1, 4, 3, 0, 0, 1, 0],
                       [21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21],
                       {"P": P,
                        "facevarying float[2] st": [0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1,
                                                    1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0,
                                                    0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1,
                                                    1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0,
                                                    0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1,
                                                    1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0],
                        "uniform integer faceid": [0, 1, 2, 3, 4, 5, 6, 7, 8]})

    ri.ReadArchive("tail.rib")
    ri.End()

prman.Init(["-progress", "-t:4"])
ri = prman.Ri()
for framenum in range(0, 30):
    test(framenum, 1)
for framenum in range(0, 30):
    test(framenum, 0)
