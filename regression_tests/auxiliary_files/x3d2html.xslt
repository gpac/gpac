<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
                              xmlns:xs="http://www.w3.org/2001/XMLSchema" >
                              
	<xsl:output method="html" version="1.0" encoding="UTF-8" indent="yes"/>
	
	<xsl:param name="filename"/>
	<xsl:param name="previous"/>
	<xsl:param name="next"/>
	<xsl:param name="use3d"/>
	
	<xsl:param name="title" select="//WorldInfo/@title"/>
	
	<xsl:template match="/">
	<html>
		<head>
			<title><xsl:value-of select="$title"/></title>
        	<link rel="stylesheet" type="text/css" href="index.css" />
		</head>
		<body>
			<h1 id="title"><xsl:value-of select="$title"/></h1>
			<div id="nav">
    			<ul>			
        			<xsl:if test="$previous"><li><a href="{$previous}.html">previous</a></li></xsl:if>
        			<li><a href="index.html">index</a></li>
        			<xsl:if test="$next"><li><a href="{$next}.html">next</a></li></xsl:if>
    			</ul>
    		</div>
			<div id="content">
								
				<div id="description">
					<h2>Description</h2>
					<xsl:call-template name="WorldInfoDescriptionToParagraph">
					    <xsl:with-param name="string" select="substring-after(//WorldInfo/@info, '&quot;')"/>
					</xsl:call-template>
				</div>
				
				<div id="contentview">
    				<h2>Content</h2>
    				<div id="downloadbar">
    					<ul>
    						<li><a href="{$filename}.x3dv">Download X3DV file</a></li>
    						<li><a href="{$filename}.x3d">Download X3D file</a></li>
    					</ul>
    				</div>
    				<embed id="player" src="{$filename}.mp4" width="320"
    				                             height="240"
    				                             type="application/x-gpac"
    				                             pluginspage="http://perso.enst.fr/~lefeuvre/GPAC/GPAC%20Framework%200.4.1%20Setup.exe" >
    				    <xsl:if test="$use3d">
    				        <xsl:attribute name="use3d">true</xsl:attribute>
    				    </xsl:if>
                    </embed>
                    <form name="formname">
                        <input type="button" value="Play" onclick='Play(document.player)'/>
                        <input type="button" value="Pause" onclick='Pause(document.player)'/>
                        <input type="button" value="Reload" onclick='Reload(document.player)'/>
                    </form>
				</div>
				<div id="snapshotview">
    				<h2>Snapshots</h2>
				</div>


				
				<div id="codeview">
				    <h2>Codes X3DV &amp; X3D</h2>
					<iframe frameborder="0" src="{$filename}.x3dv">
					</iframe>
					<iframe frameborder="0" src="{$filename}.x3d">
					</iframe>
				</div>
			</div>
		</body>
	</html>
	</xsl:template>
	
	<xsl:template name="WorldInfoDescriptionToParagraph">
	    <xsl:param name="string"/>
	    <xsl:choose>
	        <xsl:when test="contains($string,'&quot; &quot;')">
	            <xsl:value-of select="substring-before($string,'&quot; &quot;')"/><br/>
	            <xsl:call-template name="WorldInfoDescriptionToParagraph">
					<xsl:with-param name="string" select="substring-after($string,'&quot; &quot;')"/>
				</xsl:call-template>
	        </xsl:when>
	        <xsl:otherwise>
	            <xsl:value-of select="substring-before($string,'&quot;')"/><br/>
	        </xsl:otherwise>
	    </xsl:choose>
	</xsl:template>
	
    
</xsl:stylesheet>
