<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version = "1.0"
    xmlns:xsl = "http://www.w3.org/1999/XSL/Transform"
    xmlns:xs = "http://www.w3.org/2001/XMLSchema"
    xmlns:fn = "http://www.w3.org/2005/xpath-functions"
    xmlns:xdt = "http://www.w3.org/2005/xpath-datatypes"
    xmlns:xmt = "urn:mpeg:mpeg4:xmta:schema:2002">
    <xsl:output
        method = "text"
        version = "1.0"
        encoding = "UTF-8"
        indent = "yes"/>

    <xsl:param name = "xslt_path"/>

    <xsl:template match = "/">
#!/bin/sh
#xslt path: <xsl:value-of select = "$xslt_path"/>

        <xsl:apply-templates select = "//file"/>
    </xsl:template>

    <xsl:template match = "file">
        <xsl:if test = "@type='svg'">
if [ ! -e <xsl:value-of select = "@name"/>.mp4 ]; then MP4Box -mp4 <xsl:value-of select = "@name"/>.<xsl:value-of select = "@type"/>
fi
            <xsl:if test = "snapshot">
MP4Client -bmp <xsl:for-each select = "snapshot"><xsl:value-of select = "@time"/>-</xsl:for-each> <xsl:text> </xsl:text> <xsl:value-of select = "@name"/>.mp4 -2d
            </xsl:if>
            <xsl:if test = "not(@generate-html) or @generate-html != 'false'">
echo "Creating HTML for <xsl:value-of select = "@name"/>"
if [ ! -e <xsl:value-of select = "@name"/>.html ]; then 
xsltproc -o <xsl:value-of select = "@name"/>.html --stringparam filename <xsl:value-of select = "@name"/> <xsl:if test = "following::file[1]"> --stringparam next <xsl:value-of select = "following::file[1]/@name"/></xsl:if><xsl:if test= "preceding::file[1]"> --stringparam previous <xsl:value-of select = "preceding::file[1]/@name"/></xsl:if><xsl:for-each select = "snapshot"> --stringparam snapshot<xsl:value-of select = "position()"/><xsl:text> </xsl:text><xsl:value-of select = "@time"/></xsl:for-each> <xsl:value-of select = "$xslt_path"/>/svg2html.xslt <xsl:value-of select = "@name"/>.svg
fi
#echo "done."
            </xsl:if>
        </xsl:if>
        <xsl:if test = "@type='bt'">
if [ ! -e <xsl:value-of select = "@name"/>.mp4 ]; then
MP4Box -mp4 <xsl:value-of select = "@name"/>.<xsl:value-of select = "@type"/>
fi
if [ ! -e <xsl:value-of select = "@name"/>.xmt ]; then
MP4Box -xmt <xsl:value-of select = "@name"/>.<xsl:value-of select = "@type"/>
fi
            <xsl:if test = "snapshot">
MP4Client -bmp <xsl:for-each select = "snapshot"><xsl:value-of select = "@time"/>-</xsl:for-each><xsl:text> </xsl:text><xsl:value-of select = "@name"/>.mp4<xsl:choose><xsl:when test = "@use3d = 'true'"> -3d</xsl:when><xsl:otherwise> -2d</xsl:otherwise></xsl:choose>
            </xsl:if>
            <xsl:if test = "not(@generate-html) or @generate-html != 'false'">
echo "Creating HTML for <xsl:value-of select = "@name"/>"
if [ ! -e <xsl:value-of select = "@name"/>.html ]; then 
xsltproc --stringparam filename <xsl:value-of select = "@name"/><xsl:if test = "@use3d = 'true'"> --stringparam use3d true</xsl:if><xsl:if test = "following::file[1]"> --stringparam next <xsl:value-of select = "following::file[1]/@name"/></xsl:if><xsl:if test = "preceding::file[1]"> --stringparam previous <xsl:value-of select = "preceding::file[1]/@name"/></xsl:if><xsl:for-each select = "snapshot"> --stringparam snapshot<xsl:value-of select = "position()"/><xsl:text> </xsl:text><xsl:value-of select = "@time"/></xsl:for-each> -o <xsl:value-of select = "@name"/>.html <xsl:value-of select = "$xslt_path"/>/xmt2html.xslt <xsl:value-of select = "@name"/>.xmt
fi
#echo "done."
            </xsl:if>
        </xsl:if>
        <xsl:if test = "@type='x3dv'">
if [ ! -e <xsl:value-of select = "@name"/>.x3d ]; then
MP4Box -x3d <xsl:value-of select = "@name"/>.<xsl:value-of select = "@type"/>
fi
            <xsl:if test = "not(@generate-html) or @generate-html != 'false'">
echo "Creating HTML for <xsl:value-of select = "@name"/>"
if [ ! -e <xsl:value-of select = "@name"/>.html ]; then
xsltproc --stringparam filename <xsl:value-of select = "@name"/><xsl:if test = "@use3d = 'true'"> --stringparam use3d true</xsl:if><xsl:if test = "following::file[1]"> --stringparam next <xsl:value-of select = "following::file[1]/@name"/></xsl:if><xsl:if test = "preceding::file[1]"> --stringparam previous <xsl:value-of select = "preceding::file[1]/@name"/></xsl:if> -o <xsl:value-of select = "@name"/>.html <xsl:value-of select = "$xslt_path"/>/x3d2html.xslt <xsl:value-of select = "@name"/>.x3d
#echo "done."
fi
            </xsl:if>
        </xsl:if>
    </xsl:template>
</xsl:stylesheet>

