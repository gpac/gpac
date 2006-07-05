<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version = "2.0"
    xmlns:xsl = "http://www.w3.org/1999/XSL/Transform"
    xmlns:xs = "http://www.w3.org/2001/XMLSchema"
    xmlns:svg = "http://www.w3.org/2000/svg">
    <xsl:output
        method = "html"
        version = "1.0"
        encoding = "UTF-8"
        indent = "yes"/>
    <xsl:param name = "filename"/>
    <xsl:param name = "previous"/>
    <xsl:param name = "next"/>
    <xsl:param name = "title" select = "//svg:title"/>
    <xsl:template match = "/">
        <html>
            <head>
                <title>
                    <xsl:value-of select = "$title"/>
                </title>
                <link
                    rel = "stylesheet"
                    type = "text/css"
                    href = "index.css"/>
            </head>
            <body>
                <h1 id = "title">
                    <xsl:value-of select = "$title"/>
                </h1>
                <div id = "nav">
                    <ul>
                        <xsl:if test = "$previous">
                            <li>
                                <a href = "{$previous}.html">previous</a>
                            </li>
                        </xsl:if>
                        <li>
                            <a href = "index.html">index</a>
                        </li>
                        <xsl:if test = "$next">
                            <li>
                                <a href = "{$next}.html">next</a>
                            </li>
                        </xsl:if>
                    </ul>
                </div>
                <div id = "content">
                    <div id = "description">
                        <h2>Description</h2>
                        <xsl:value-of select = "//svg:desc"/>
                    </div>
                    <div id = "contentview">
                        <h2>Content</h2>
                        <div id = "downloadbar">
                            <ul>
                                <li>
                                    <a href = "{$filename}.svg">Download SVG file</a>
                                </li>
                                <li>
                                    <a href = "{$filename}.mp4">Download LASeR file (MP4/LASeR)</a>
                                </li>
                            </ul>
                        </div>
                        <object
                            id = "player"
                            width = "{//svg:svg/@width}"
                            height = "{//svg:svg/@height}"
                            type = "application/x-gpac"
                            pluginspage = "http://perso.enst.fr/~lefeuvre/GPAC/GPAC%20Framework%200.4.1%20Setup.exe">
                            <param name = "src" value = "{$filename}.svg"/>
                            Your browser does not have the GPAC plugin, go to http://gpac.sourceforge.net ...
                        </object>
                        <form name = "formname">
                            <input
                                type = "button"
                                value = "Play"
                                onclick = "Play(document.player)"/>
                            <input
                                type = "button"
                                value = "Pause"
                                onclick = "Pause(document.player)"/>
                            <input
                                type = "button"
                                value = "Reload"
                                onclick = "Reload(document.player)"/>
                        </form>
                    </div>
                    <div id = "snapshotview">
                        <h2>Snapshots</h2>
                    </div>
                    <div id = "codeview">
                        <h2>SVG Code</h2>
                        <iframe frameborder = "0" src = "{$filename}.svg"></iframe>
                    </div>
                </div>
            </body>
        </html>
    </xsl:template>
</xsl:stylesheet>
