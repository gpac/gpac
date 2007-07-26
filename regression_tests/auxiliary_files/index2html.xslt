<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:fn="http://www.w3.org/2005/xpath-functions" xmlns:xdt="http://www.w3.org/2005/xpath-datatypes">
	<xsl:output method="xml" version="1.0" encoding="UTF-8" indent="yes"/>
	
		<xsl:template match="/">
			<html>
				<head>
					<META http-equiv="Content-Type" content="text/html;" charset="UTF-8"/>
					<title>GPAC Non-regression Test Suite Navigator</title>
					<link href="index.css" type="text/css" rel="stylesheet"/>
				</head>
				<body>

<p>The suite contains tests for:</p>
<ul>
<li>all BIFS nodes supported by the GPAC renderer</li>
<li>all BIFS commands, extended BIFS commands and OD Commands</li>
<li>Stream Management (MediaControl, MediaSegments and segmentDescriptor usage)</li>
<li>Most BIFS Advanced Text and Graphics nodes</li>
</ul>

<p>Test sequences are provided in the BT format, and need MP4Box to be encoded or translated to XMT-A. 
<br/>The GPAC Regression Tests are part of the <a href="../home_download.php">GPAC source code</a> and are also available on <a href="http://gpac.cvs.sourceforge.net/gpac/gpac/regression_tests">GPAC CVS</a>.</p>

<!--
					<h1 id="title">GPAC Non-regression Test Suite Navigator</h1>
-->					<div id="content">
						<xsl:apply-templates/>
					</div>
				</body>
			</html>
		</xsl:template>
		
		<xsl:template match="format">
		    <h2><xsl:value-of select="@name"/></h2>
		    <div id="format">
    			<ul>			
    				<xsl:apply-templates select="category"/>
    			</ul>
		    </div>
		</xsl:template>

		<xsl:template match="category">
		    <h3><xsl:value-of select="@name"/></h3>
		    <div id="category">
    			<ul>			
    				<xsl:apply-templates select="file"/>
    			</ul>
		    </div>
		</xsl:template>

		<xsl:template match="file">
            <xsl:if test="not(@generate-html)or @generate-html != 'false'">
    			<li><a href="{@name}.html">
    			<xsl:choose>
    			    <xsl:when test="@title"><xsl:value-of select="@title"/></xsl:when>
    			    <xsl:otherwise><xsl:value-of select="@name"/></xsl:otherwise>
    			</xsl:choose>
    			</a></li>
    		</xsl:if>
		</xsl:template>
</xsl:stylesheet>
