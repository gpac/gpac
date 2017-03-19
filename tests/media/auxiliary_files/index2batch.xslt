<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet
    version = "2.0"
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
    <xsl:param name = "xalan_path"/>
    <xsl:param name = "xslt_path"/>
    <xsl:template match = "/">
@ECHO OFF
SET XALAN="<xsl:value-of select = "$xalan_path"/>"
        <xsl:apply-templates select = "//file"/>
    </xsl:template>
    <xsl:template match = "file">
        <xsl:if test = "@type='svg'">
IF NOT EXIST <xsl:value-of select = "@name"/>.mp4 MP4Box -mp4 <xsl:value-of select = "@name"/>.<xsl:value-of select = "@type"/>
            <xsl:if test = "snapshot">
C:\Users\Cyril\sourceforge\gpac\bin\w32_deb\mp4client -bmp <xsl:for-each select = "snapshot"><xsl:value-of select = "@time"/>-</xsl:for-each> <xsl:text> </xsl:text> <xsl:value-of select = "@name"/>.mp4 -2d
            </xsl:if>
            <xsl:if test = "not(@generate-html) or @generate-html != 'false'">
@echo Creating HTML for <xsl:value-of select = "@name"/>
IF NOT EXIST <xsl:value-of select = "@name"/>.html java -jar %XALAN% -IN <xsl:value-of select = "@name"/>.svg -OUT <xsl:value-of select = "@name"/>.html -XSL <xsl:value-of select = "$xslt_path"/>/svg2html.xslt -PARAM filename <xsl:value-of select = "@name"/> <xsl:if test = "following::file[1]"> -PARAM next <xsl:value-of select = "following::file[1]/@name"/></xsl:if><xsl:if test = "preceding::file[1]"> -PARAM previous <xsl:value-of select = "preceding::file[1]/@name"/></xsl:if><xsl:for-each select = "snapshot"> -PARAM snapshot<xsl:value-of select = "position()"/><xsl:text> </xsl:text><xsl:value-of select = "@time"/></xsl:for-each>
@echo done.
            </xsl:if>
        </xsl:if>
        <xsl:if test = "@type='bt'">
IF NOT EXIST <xsl:value-of select = "@name"/>.mp4 MP4Box -mp4 <xsl:value-of select = "@name"/>.<xsl:value-of select = "@type"/>
IF NOT EXIST <xsl:value-of select = "@name"/>.xmt MP4Box -xmt <xsl:value-of select = "@name"/>.<xsl:value-of select = "@type"/>
            <xsl:if test = "snapshot">
C:\Users\Cyril\sourceforge\gpac\bin\w32_deb\mp4client -bmp <xsl:for-each select = "snapshot"><xsl:value-of select = "@time"/>-</xsl:for-each><xsl:text> </xsl:text><xsl:value-of select = "@name"/>.mp4<xsl:choose><xsl:when test = "@use3d = 'true'"> -3d</xsl:when><xsl:otherwise> -2d</xsl:otherwise></xsl:choose>
            </xsl:if>
            <xsl:if test = "not(@generate-html) or @generate-html != 'false'">
@echo Creating HTML for <xsl:value-of select = "@name"/>
IF NOT EXIST <xsl:value-of select = "@name"/>.html java -jar %XALAN% -IN <xsl:value-of select = "@name"/>.xmt -OUT <xsl:value-of select = "@name"/>.html -XSL <xsl:value-of select = "$xslt_path"/>/xmt2html.xslt -PARAM filename <xsl:value-of select = "@name"/><xsl:if test = "@use3d = 'true'"> -PARAM use3d true</xsl:if><xsl:if test = "following::file[1]"> -PARAM next <xsl:value-of select = "following::file[1]/@name"/></xsl:if><xsl:if test = "preceding::file[1]"> -PARAM previous <xsl:value-of select = "preceding::file[1]/@name"/></xsl:if><xsl:for-each select = "snapshot"> -PARAM snapshot<xsl:value-of select = "position()"/><xsl:text> </xsl:text><xsl:value-of select = "@time"/></xsl:for-each>
@echo done.
            </xsl:if>
        </xsl:if>
        <xsl:if test = "@type='x3dv'">
IF NOT EXIST <xsl:value-of select = "@name"/>.x3d MP4Box -x3d <xsl:value-of select = "@name"/>.<xsl:value-of select = "@type"/>
            <xsl:if test = "not(@generate-html) or @generate-html != 'false'">
@echo Creating HTML for <xsl:value-of select = "@name"/>
IF NOT EXIST <xsl:value-of select = "@name"/>.html java -jar %XALAN% -IN <xsl:value-of select = "@name"/>.x3d -OUT <xsl:value-of select = "@name"/>.html -XSL <xsl:value-of select = "$xslt_path"/>/x3d2html.xslt -PARAM filename <xsl:value-of select = "@name"/><xsl:if test = "@use3d = 'true'"> -PARAM use3d true</xsl:if><xsl:if test = "following::file[1]"> -PARAM next <xsl:value-of select = "following::file[1]/@name"/></xsl:if><xsl:if test = "preceding::file[1]"> -PARAM previous <xsl:value-of select = "preceding::file[1]/@name"/></xsl:if>
@echo done.
            </xsl:if>
        </xsl:if>
    </xsl:template>
</xsl:stylesheet>
