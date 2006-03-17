<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:fn="http://www.w3.org/2005/xpath-functions" xmlns:xdt="http://www.w3.org/2005/xpath-datatypes">
	<xsl:output method="xml" version="1.0" encoding="UTF-8" indent="yes"/>
	
		<xsl:template match="/">
			<html>
				<head>
					<META http-equiv="Content-Type" content="text/html;" charset="UTF-8"/>
					<title>GPAC Non-regression Test Suite Navigator</title>
					<link href="index.css" type="text/css" rel="stylesheet"/>
				</head>
				<body>
					<h1 id="title">GPAC Non-regression Test Suite Navigator</h1>
					<div id="content">
						<xsl:apply-templates select="//format"/>
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
    			<li><a href="{@name}.html"><xsl:value-of select="@name"/></a></li>
    		</xsl:if>
		</xsl:template>
</xsl:stylesheet>
