#!/usr/bin/env python
from string import Template

main='''
  <section>
    <div class="left">
    <h1>Ptex</h1>
    </div>
    <div class="right">
        <p><a href="http://github.com/wdas/ptex"><button class="btn btn-light">GITHUB</button></a>
        <a href="documentation.html#license"><button class="btn btn-light">LICENSE</button></a></p>

        <p><b>Ptex</b> is a texture mapping system developed by <a href="http://www.disneyanimation.com" target="_top">Walt Disney Animation Studios</a> for production-quality rendering:</p>
        <ul>
        <li>No UV assignment is required!  Ptex applies a separate texture to each face of a subdivision or polygon mesh.</li>
        <li>The Ptex <a href="documentation.html#fileformat">file format</a> can efficiently store hundreds of thousands of texture images in a single file.</li>
        <li>The Ptex <a href="documentation.html#api">API</a> provides cached file I/O and high-quality filtering -
        everything that is needed to easily add Ptex support to a production-quality renderer or texture authoring application. </li>
        </ul>
    </div>
  </section>
  <section>
   <div class="left">
    <h2>News</h2>
   </div>
   <div class="right">
   <p>$news</p>
   </div>
  </section>
'''

newsitems = [
    ['Jul 16, 2018',
     '<a href="http://pharr.org/matt/blog/2018/07/16/moana-island-pbrt-5.html">Ptex performance evaluated on 96 threads</a> using Moana data in PBRT.'],
    ['Jul 4, 2018',
     '<a href="https://www.disneyanimation.com/technology/datasets>">Production data set</a> from Moana including Ptex textures was publicly released for research use.'],
    ['Oct 5, 2011',
     'Ptex supported in <a href="http://www.3delight.com">3Delight Studio Pro 10</a>.'],
    ['Aug 11, 2011',
     '<a href="http://developer.nvidia.com/siggraph-2011/">Real-time Ptex</a> demonstrated by NVIDIA at Siggraph 2011.'],
    ['July 22, 2011',
     'Ptex supported in <a href="http://www.sitexgraphics.com/html/air_11_press_release.html">Air 11</a>.'],
    ['May 31, 2011',
     'Ptex supported in <a href="https://www.foundry.com/products/mari">Mari 1.3</a>.'],
    ['Apr 29, 2011',
     'Ptex supported in <a href="http://www.chaosgroup.com">V-Ray 2.0</a>.'],
    ['Sep 29, 2010',
     'Ptex export supported in <a href="https://www.autodesk.com/products/mudbox/overview?plc=MBXPRO">Mudbox 2011 SAP</a>.'],
    ['Aug 24, 2010',
     'Ptex supported in <a href="http://www.3d-coat.com/">3D-Coat 3.3</a>.'],
    ['July 27, 2010',
     'Ptex supported in <a href="http://www.sidefx.com/index.php">Houdini 11</a>.'],
    ['Jan 15, 2010',
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
<table>
<col width=120> <col>
%s
</table>
''' % ('\n'.join(['<tr><td valign="top">%s</td><td>%s</td></tr>' % (d,i) for d,i in newsitems]))

main = Template(main).safe_substitute(news=news)

overview='''

<section>
<div class="left">
<a href="http://media.disneyanimation.com/technology/opensource/ptex/ptex-teaser-big.png"><img border=0 src="ptex-teaser.png" alt=""></a>
</div>
<div class="right">
<p>Model with 2694 faces rendered with Ptex.  No explicit UV assignment was used.  The largest texture layer, the fine-scale displacements, has 836 million texels stored in a single Ptex file with individual face resolutions ranging from 64 x 64 to 2048 x 2048 texels.
No seams are visible across faces, even under <a href="http://media.disneyanimation.com/technology/opensource/ptex/ptex-teaser-big.png">close magnification</a>.<br>(&copy; Walt Disney Animation Studios)</a></p>
</div>
</section>

<section>
<div class="left">
<h2>Motivation</h2>
</div>

<div class="right">
<p>There were many drawbacks to traditional texture mapping methods that led to the development of Ptex:</p>
<ul>
<li>UV assignment was a tedious task, and making good UVs on complex models was difficult.</li>
<li>Texture seams could produce visible artifacts especially with displacement maps.</li>
<li>Large numbers of texture files were required creating a significant I/O bottleneck.</li>
</ul>
<p>Ptex addresses all these issues by eliminating UV assignment, providing seamless filtering, and allowing any number of textures to be stored in a single file.</p>
<p>Ptex has been used on virtually every surface of every Walt Disney Animation Studios production since 2008.</p>
</div>
</section>

<section>
<div class="left">
<h2>Major Features</h2>
</div>

<div class="right">
<ul>
<li>Supports Catmull-Clark subdivision surfaces (including quad and non-quad faces), Loop subdivision surfaces, and polymeshes (either all-quad or all-triangle).</li>
<li>Several data types are supported including 8 or 16-bit integer, float, and half-precision float.</li>
<li>An arbitrary number of channels can be stored in a Ptex file.</li>
<li>Arbitrary meta data can be stored in the Ptex file and accessed through the memory-managed cache.</li>
</ul>
</div>
</section>

'''

documentation='''
<section>
<div class="left">
<h2 style="margin-top:0px">Technical Details</h2>
</div>
<div class="right"
<ul>
    <li>See the <a href="ptexpaper.html">paper</a>, <i>Ptex: Per-Face Texture Mapping for Production Rendering</i>, Brent Burley and Dylan Lacewell, Eurographics Symposium on Rendering 2008.
    <li>View the slides from the Eurographics presentation as <a href="http://media.disneyanimation.com/technology/opensource/ptex/ptex.ppt">Powerpoint</a> or <a href="http://media.disneyanimation.com/technology/opensource/ptex/ptex-slides.pdf">pdf</a>.</li>
</ul>
</div>
</section>

<section>
<div class="left">
<h2>Workflow</h2>
</div>
<div class="right"
<ul>
    <li>Watch a YouTube video, demonstrating the <a href="http://www.youtube.com/watch?v=GxNlAlOuQQQ">Ptex painting workflow</a>.  <br><i>Note: the application shown in the video is not part of the Ptex open source package.</i></li>
</ul>
</div>
<a id="fileformat"></a>
</section>

<section>
<div class="left">
<h2>Ptex File Format</h2>
</div>
<div class="right"
<p>Ptex uses a custom file format that is designed specifically for efficient rendering.
A Ptex file can store an arbitrary number of textures along with mipmaps and adjacency data used for filtering across face boundaries.</p>
<ul>
<li><a href="PtexFile.html">Ptex File Format Specification</a></li>
<li><a href="adjdata.html">Adjacency data</a></li>
<li><a href="tritex.html">Triangle textures</a></li>
<li><a href="metakeys.html">Standard meta data keys</a></li>
</ul>
<a id="api"></a>
</section>

<section>
<div class="left">
<h2>Ptex API</h2>
</div>
<div class="right">
<p>The Ptex API is written in C++ and can peform file I/O, caching, and filtering needed for rendering.<br>
See the <a href="apidocs/index.html">Ptex API docs</a> for details.</p>
</div>
<a id="license"></a>
</section>

<section>
<div class="left">
<h2>Ptex License</h2>
</div>
<div class="right"
<blockquote>
PTEX SOFTWARE<br>
Copyright 2018 Disney Enterprises, Inc. All rights reserved<br><br>

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
</div>
</section>
'''

releases = [
    ['v2.0.0', 'Nov 23, 2009', 'Initial release'],
    ['v2.0.22', 'May 28, 2010', 'Windows compatibility fixes, minor bug fixes'],
    ['v2.0.29', 'May 2, 2011', 'Minor bug fixes. API change: added PtexPtr::reset method.'],
    ['v2.0.30', 'May 9, 2011', 'Critical bug fix: threading crash when using multiple caches.'],
    ['v2.0.33', 'Dec 14, 2011', 'Fixed crash when rendering triangle textures that have no stored mipmaps.'],
    ['v2.0.37', 'Apr 8, 2013', 'Cmake support, compiler warnings, WIN32 export control.'],
    ['v2.0.41', 'Apr 9, 2013', 'Limit I/O buffer size, avoid float/double conversion, remove LOD bias on bilinear filter, silence warnings.'],
    ['v2.1.10', 'Dec 8, 2015', 'New cache architecture for better threading. New cache stats API. New edge filter mode for tangent-space maps. Optimizations.'],
    ['v2.1.28', 'Apr 4, 2016', 'API: MetaData::findKey, const neighborhood check improvement, build/portability fixes.'],
    ['v2.1.33', 'Jan 24, 2018', 'Security fix, build fixes.'],
    ['v2.3.0', 'Apr 10, 2018', 'Minor maintenance. Version number rollup to sync w/ in-house version.'],
    ['v2.3.2', 'Mar 8, 2019', 'Minor maintenance. Windows fixes.'],
]
releases.reverse()

download='''
<section>
<div class="left">
<H2>Read the license</H2>
<p>Read the <a href="documentation.html#license">Ptex License</a> before downloading this software.</p>
</div>

<div class="right">
<H2>Source code</H2>
<p>The Ptex API is hosted as free open source at <a href="http://github.com/wdas/ptex">github.com/wdas/ptex</a> and discussed at <a href="http://groups.google.com/group/ptex">groups.google.com/group/ptex</a>.</p>
<p>Browse the <a href="http://github.com/wdas/ptex/commits/master">change history</a>.</p>
</div>
</section>


<section>
<div class="left">
<H2>Release History</H2>
</div>
<div class="right">

Download the source:<br>
<table>
<col width=80> <col width=120> <col>
</div>
%s
</table>
</section>
''' % ('\n'.join(['<tr><td valign="top"><a href="http://github.com/wdas/ptex/archive/%s.zip">%s</a></td><td valign="top">%s</td><td>%s</td></tr>'
                  % (v,v,d,c) for v,d,c in releases]))

sampleitems=[
    ['teapot', 'Utah Teapot (Catmull-Clark subd)'],
    ['bunny', 'Stanford Bunny (Loop subd, w/ triangular texels)'],
    ['holeycube', 'Smooth displacement filtering around extraordinary points'],
    ['nonquad', 'Mixed quad/triangle mesh'],
    ['pentagon', 'Catmull-Clark subd with single pentagon face'],
    ['triangle', 'Filtered triangular texels'],
    ['envcube', 'Ptex cube maps reflected on a sphere'],
    ]
sample='''
<section>
<div class="left">
<h2>%s</h2>
<p>Download <a href="samples/%s.zip">%s.zip</a></p>
</div>
<div class="right">
<img border=0 src="samples/%s.png">
</div>
</section>
'''

samples='''
<section>
<div class="left">
<h2>Production data set</h2>
</div>
<div class="right">
<p><a href="https://www.disneyanimation.com/technology/datasets">Moana Island Scene</a> 93 GB of geometry and Ptex textures.<p/>
</div>
</section>

<section>
<div class="left">
<h2>Rendered images</h2>
</div>
<div class="right">
<p>Download a Quicktime movie showing <a href="http://media.disneyanimation.com/technology/opensource/ptex/ptexTurntables.mp4">production models</a> from Glago's Guest painted and rendered with Ptex.<p/>
</div>
</section>

%s
''' % ('\n'.join([sample % (desc,item,item,item) for item,desc in sampleitems]))


page_template='''
<!DOCTYPE html>
<html lang="en" dir="ltr">

<head>
  <meta charset="utf-8">
  <title>$title</title>
  <link rel="icon" href="PtexCube16.png">
  <link rel="stylesheet" href="style.css">
</head>

<body>
  <nav>
    <div class="software">
      <a href="index.html" class="software"><img src="PtexCubeOrange64.png" border=0></a>
    </div>
    <div class="navigation"> $nav </div>
  </nav>
$body
</body>
<footer>
''' + open("wdaslogo.html").read() + '''
</footer>
</html>
'''

navitems = [["Overview", "overview.html"],
            ["Documentation", "documentation.html"],
            ["Samples", "samples.html"],
            ["Download", "download.html"]]

nav = ' '.join(['<a class="navigation" href="%s">%s</a>' %
                 (href,text) for text,href in navitems])

def genpage(name, title, heading, body):
    open(name, 'w').write(Template(page_template).safe_substitute(
        title=title,
        heading=heading,
        body=body,
        nav=nav
        ))

genpage("index.html", "Ptex", "&nbsp;", main)
genpage("overview.html", "Ptex Overview", "Overview", overview)
genpage("documentation.html", "Ptex Documentation", "Documentation", documentation)
genpage("samples.html", "Ptex Samples", "Samples", samples)
genpage("download.html", "Ptex Download", "Download", download)
