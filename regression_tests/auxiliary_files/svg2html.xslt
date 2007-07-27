<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version = "1.0"
    xmlns:xsl = "http://www.w3.org/1999/XSL/Transform"
    xmlns:xs = "http://www.w3.org/2001/XMLSchema"
    xmlns:svg = "http://www.w3.org/2000/svg">
    <xsl:output method = "xml" version = "1.0" encoding = "UTF-8" indent = "yes"/>
    <xsl:param name = "filename"/>
    <xsl:param name = "previous"/>
    <xsl:param name = "next"/>
    <xsl:param name = "use3d"/>
    <xsl:param name = "snapshot1"/>
    <xsl:param name = "snapshot2"/>
    <xsl:param name = "snapshot3"/>
    <xsl:param name = "snapshot4"/>
    <xsl:param name = "snapshot5"/>
    <xsl:param name = "snapshot6"/>
    <xsl:param name = "title" select = "//svg:title"/>
    <xsl:template match = "/">
        <html>
            <head>
                <title>
                    <xsl:value-of select = "$title"/>
                </title>
                <link rel = "stylesheet" type = "text/css" href = "index.css"/>
            </head>
            <body>
                <h1 id = "title">
                    <xsl:value-of select = "$title"/>
                </h1>
                <div id = "nav">
                    <ul>
                        <xsl:if test = "$previous">
                            <li>
                                <a href = "{$previous}.html">Previous</a>
                            </li>
                        </xsl:if>
                        <li>
                            <a href = "index.html">Index</a>
                        </li>
                        <xsl:if test = "$next">
                            <li>
                                <a href = "{$next}.html">Next</a>
                            </li>
                        </xsl:if>
                    </ul>
                </div>
                <div id = "content">
                    <div id = "left">                
                        <div id = "downloadbar">
                            <h2>Download</h2>
                            <ul>
                                <li><a href = "{$filename}.svg">SVG </a></li>
                                <li><a href = "{$filename}.xsr">LASeRML </a></li>
                                <li><a href = "{$filename}.mp4">LASeR (as MP4 file)</a></li>
                                <li><a href = "{$filename}.saf">LASeR in SAF </a></li>
                            </ul>
                        </div>
                        <div id = "description">
                            <h2>Description</h2>
                            <xsl:value-of select = "//svg:desc"/>
                        </div>
                    </div>
                    <div id = "right">                
                        <div id = "contentview">
                            <h2>Viewer</h2>
                            <object id = "player" type = "application/x-gpac"
                                width = "100%"
                                height = "100%"                                
                                pluginspage = "http://tsi.enst.fr/~lefeuvre/GPAC/">
                                <param name = "src" value = "{$filename}.svg"/>
Your browser does not have the GPAC plugin installed, visit http://gpac.sourceforge.net for more information ...</object>
                            <!--form name = "formname">
                                <input type = "button" value = "Play/Pause" onclick = "document.player.Pause()"/>
                            </form-->
                        </div>
                    </div>
                    <xsl:if test = "$snapshot1">
                        <div id = "snapshotview">
                            <h2>Snapshots</h2>
                            <table>
                                <tr>
                                    <xsl:if test = "$snapshot1"><td>At time T = <xsl:value-of select="$snapshot1"/></td></xsl:if>
                                    <xsl:if test = "$snapshot2"><td>At time T = <xsl:value-of select="$snapshot2"/></td></xsl:if>
                                    <xsl:if test = "$snapshot3"><td>At time T = <xsl:value-of select="$snapshot3"/></td></xsl:if>
                                    <xsl:if test = "$snapshot4"><td>At time T = <xsl:value-of select="$snapshot4"/></td></xsl:if>
                                    <xsl:if test = "$snapshot5"><td>At time T = <xsl:value-of select="$snapshot5"/></td></xsl:if>
                                    <xsl:if test = "$snapshot6"><td>At time T = <xsl:value-of select="$snapshot6"/></td></xsl:if>
                                </tr>
                                <tr>
                                    <xsl:if test = "$snapshot1">
                                        <td><img src = "{$filename}_1.bmp" alt = "{concat('Snapshot #1 at time ',$snapshot1)}"/></td>
                                    </xsl:if>
                                    <xsl:if test = "$snapshot2">
                                        <td><img src = "{$filename}_2.bmp" alt = "{concat('Snapshot #2 at time ',$snapshot2)}"/></td>
                                    </xsl:if>
                                    <xsl:if test = "$snapshot3">
                                        <td><img src = "{$filename}_3.bmp" alt = "{concat('Snapshot #3 at time ',$snapshot3)}"/></td>
                                    </xsl:if>
                                    <xsl:if test = "$snapshot4">
                                        <td><img src = "{$filename}_4.bmp" alt = "{concat('Snapshot #4 at time ',$snapshot4)}"/></td>
                                    </xsl:if>
                                    <xsl:if test = "$snapshot5">
                                        <td><img src = "{$filename}_5.bmp" alt = "{concat('Snapshot #5 at time ',$snapshot5)}"/></td>
                                    </xsl:if>
                                    <xsl:if test = "$snapshot6">
                                        <td><img src = "{$filename}_6.bmp" alt = "{concat('Snapshot #6 at time ',$snapshot6)}"/></td>
                                    </xsl:if>
                                </tr>
                            </table>
                        </div>
                    </xsl:if>
                </div>
            </body>
        </html>
    </xsl:template>
</xsl:stylesheet>
