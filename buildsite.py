#!/usr/bin/env python
from string import Template

main='''
<b>Ptex</b> is a texture mapping system developed
by <a href="http://disneyanimation.com" target="_top">Walt Disney
Animation Studios</a> for production-quality rendering:
<ul>
<li class="pad">No UV assignment is required!  Ptex applies a
separate texture to each face of a subdivision or polygon mesh.</li>
<li class="pad">The Ptex <a href="documentation.html#fileformat">file format</a> can efficiently store hundreds of thousands of texture images in a single file.</li>
<li class="pad">The Ptex <a href="documentation.html#api">API</a> provides cached file I/O and high-quality filtering -
everything that is needed to easily add Ptex support to a production-quality renderer or texture authoring application. </li>
</ul>

<br>
News<hr width=100%>
$news

<br>
<br>
<center>
<a target="_top" href="http://www.disneyanimation.com">
<img src="wdaslogo.png" border=0></a>
</center>
'''

newsitems = [['Jan 15, 2010',
              'Ptex released as free open source.'],
             ['Sep 30, 2009',
              'Ptex integrated into <a href="https://renderman.pixar.com/">Pixar\'s RenderMan Pro Server 15.0</a>.'],
             ['Nov 21, 2008',
              'Disney\'s Bolt released; first feature film to use Ptex.'],
             ['Jun 24, 2008',
              'Ptex presented at Eurographics Symposium on Rendering.'],
             ['Jun 10, 2008',
              'Glago\'s Guest premiered at Annecy; first production to use Ptex.']]

news = '''
<table style="table-layout:fixed;width:600px;">
<col width=120> <col>
%s
</table>
''' % ('\n'.join(['<tr><td>%s</td><td>%s</td></tr>' % (d,i) for d,i in newsitems]))

main = Template(main).safe_substitute(news=news)

overview='''

<a href="http://www.disneyanimation.com/library/ptex/ptex-teaser-big.png"><img border=0 src="ptex-teaser.png" alt=""></a>

<br> Model with 2694 faces rendered with Ptex.  No explicit UV
assignment was used.  The largest texture layer, the fine-scale
displacements, has 836 million texels stored in a single Ptex file
with individual face resolutions ranging from 64 x 64 to 2048 x 2048
texels.  No seams are visible across faces, even under <a
href="http://www.disneyanimation.com/library/ptex/ptex-teaser-big.png">close
magnification</a>.<br>(&copy; Walt Disney Animation Studios)

<h2>Motivation</h2>

There were many drawbacks to traditional texture mapping methods that led to the development of Ptex:
<ul>
<li>UV assignment was a tedious task, and making good UVs on complex models was difficult.</li>
<li>Texture seams could produce visible artifacts especially with displacement maps.</li>
<li>Large numbers of texture files were required creating a significant I/O bottleneck.</li>
</ul>

Ptex addresses all these issues by eliminating UV assignment,
providing seamless filtering, and allowing any number of textures to
be stored in a single file. <br><br>

Ptex was used on virtually every surface
in the feature film Bolt, and is now the primary texture mapping
method for all productions at Walt Disney Animation Studios.


<h2>Major Features</h2>

<ul>
<li>Supports Catmull-Clark subdivision surfaces (including quad and non-quad faces), Loop subdivision surfaces, and polymeshes (either all-quad or all-triangle).</li>
<li>Several data types are supported including 8 or 16-bit integer, float, and half-precision float.</li>
<li>An arbitrary number of channels can be stored in a Ptex file.</li>
<li>Arbitrary meta data can be stored in the Ptex file and accessed through the memory-managed cache.</li>
</ul>
<br>
'''

documentation='''
<h2 style="margin-top:0px">Technical Details</h2>
<ul>
    <li>See the <a href="ptexpaper.html">paper</a>, 
<i>Ptex: Per-Face Texture Mapping for Production Rendering</i>, Brent Burley and Dylan Lacewell, Eurographics Symposium on Rendering 2008.
    <li>View the slides from the Eurographics presentation as <a href="http://www.disneyanimation.com/library/ptex/ptex.ppt">Powerpoint</a> or <a href="http://www.disneyanimation.com/library/ptex/ptex-slides.pdf">pdf</a>.</li>
</ul>

<h2>Workflow</h2>
<ul>
    <li>Watch a YouTube video, demonstrating the <a href="http://www.youtube.com/watch?v=GxNlAlOuQQQ">Ptex painting workflow</a>.</li>
</li>
</ul>

<a name="fileformat"></a><h2>Ptex File Format</h2>

<blockquote>
Ptex uses a custom file format that is designed specifically for
efficient rendering.  A Ptex file can store an arbitrary number of
textures along with mipmaps and adjacency data used for filtering
across face boundaries.
</blockquote>

<ul>
<li><a href="PtexFile.html">Ptex File Format Specification</a></li>
<li><a href="adjdata.html">Adjacency data</a></li>
<li><a href="tritex.html">Triangle textures</a></li>
<li><a href="metakeys.html">Standard meta data keys</a></li>
</ul>

<a name="api"></a><h2>Ptex API</h2>
<blockquote>
The Ptex API is written in C++ and can peform file I/O, caching, and filtering needed for rendering.
See the <a href="apidocs/index.html">Ptex API</a> docs for details.
</blockquote>

<a name="license"></a><h2>Ptex License</h2>

<blockquote>
PTEX SOFTWARE<br>
Copyright 2009 Disney Enterprises, Inc. All rights reserved<br><br>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

<ul>
<li> Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

<li> Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

<li> The names "Disney", "Walt Disney Pictures", "Walt Disney Animation
     Studios" or the names of its contributors may NOT be used to
     endorse or promote products derived from this software without
     specific prior written permission from Walt Disney Pictures.
</ul>

Disclaimer: THIS SOFTWARE IS PROVIDED BY WALT DISNEY PICTURES AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE, NONINFRINGEMENT AND TITLE ARE DISCLAIMED.
IN NO EVENT SHALL WALT DISNEY PICTURES, THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND BASED ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
</blockquote>
'''

download='''
Read the <a href="documentation.html#api">Ptex License</a> before downloading this software.<br><br>
The Ptex API is hosted as free open source at <a href="http://github.com/wdas/ptex">github</a>.<br>
Download the source for the latest release, <a href="http://github.com/wdas/ptex/archives/v2.0.0">v2.0.0</a>.<br>
'''

mailinglists='''
Two mailing lists have been set up for Ptex on Google Groups:
<ul>
<li> <a href="http://groups.google.com/group/ptex"><b>ptex</b></a> - for general Ptex discussion</li>
<li> <a href="http://groups.google.com/group/ptex-announce"><b>ptex-announce</b></a> - for release announcements</li>
</ul>
'''

sampleitems=[
    ['teapot', '274K', 'Utah Teapot (Catmull-Clark subd).'],
    ['bunny', '22M', 'Stanford Bunny (Loop subd).'],
    ['holeycube', '2.3M', 'Cube w/ holes through center, rendered with displacement map (Catmull-Clark subd).'],
    ['nonquad', '494K', 'Catmull-Clark subd with fifteen quad faces and two triangle faces.'],
    ['triangle', '263K', 'Mesh with nine triangle faces mapped with 4x4 texels per face, rendered using point and gaussian filter.'],
    ['envcube', '534K', 'Ptex cube maps reflected on a sphere.'],
    ]
samples='''
Download a Quicktime movie showing
<a href="http://www.disneyanimation.com/library/ptex/ptexTurntables.mov">production
models</a> painted and rendered with Ptex (69M, JPEG encoding).<p>

Download sample Ptex projects below.  Each sample project includes a geometry obj
file, a Renderman rib file, Renderman shaders, one or more ptex textures, and
512x512 image renders corresponding to the thumbnails shown below.

<table style="table-layout:fixed;width:600px;">
<col width=300px> <col>
%s
</table>
''' % ('\n'.join(['<tr><td><a href="samples/%s.zip">%s.zip</a> (%s)<br>%s</td><td><img border=0 src="samples/%s.png"></td></tr>' %
                  (item,item,size,desc,item) for item,size,desc in sampleitems]))


page_template='''
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">
<html>
<head>
<link rel="icon" href="PtexCube16.png">
<link href="ptex.css" rel="stylesheet" type="text/css">
<title>$title</title>

<style type="text/css">
BODY { background-color:#E7E2D5; color:black;
       background-image:url('bg.gif'); background-repeat:no-repeat }
A         { color: #00005f; }
A.nav     { text-decoration: none; }
A:hover   { color: #3f3f9f; }
DIV.container { width:100%; margin:0px;}
DIV.logo    { position:absolute; left:20px; top:20px; }
DIV.nav     { position:absolute; left:0px; top:80px; width:132px; text-align:right; font-size: 14px; }
DIV.content { position:absolute; left:170px; top:80px; width:600px; }
LI.pad      { padding-top:10px; padding-bottom:10px; }

</style>

  </head>
  <body>

<a class="logo" href="index.html"><img src="PtexLogoColor64.png" border=0 alt="Ptex"></a> <br>
<div class="nav">
$hilite
$nav
</div>

<div class="content">
$body
</div>
</body>
</html>
'''

navitems = [["Overview", "overview.html"],
            ["Documentation", "documentation.html"],
            ["Samples", "samples.html"],
            ["Download", "download.html"],
            ["Mailing Lists", "mailinglists.html"]]

nav = '\n'.join(['<div class="nav" style="top:%dpx;"><a class="nav" href="%s">%s</a></div>' %
                 (i*30, n[1], n[0]) for i,n in enumerate(navitems)])
hiliteTemplate = '<div class="nav" style="top:%dpx"><img src="grad.png"></div>'


def genpage(name, title, heading, body):
    hilite = ''
    for i,n in enumerate(navitems):
        if n[1] == name:
            hilite = hiliteTemplate % (i*30 + 2)

    open(name, 'w').write(Template(page_template).safe_substitute(
        title=title,
        heading=heading,
        body=body,
        nav=nav,
        hilite=hilite
        ))

genpage("index.html", "Ptex", "&nbsp;", main)
genpage("overview.html", "Ptex Overview", "Overview", overview)
genpage("documentation.html", "Ptex Documentation", "Documentation", documentation)
genpage("samples.html", "Ptex Samples", "Samples", samples)
genpage("download.html", "Ptex Download", "Download", download)
genpage("mailinglists.html", "Ptex Mailing Lists", "Mailing Lists", mailinglists)
