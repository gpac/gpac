<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
	<xsl:output method="html" version="1.0" encoding="UTF-8" indent="yes"/>

    <xsl:template match="svg-status-query">
		<html xmlns="http://www.w3.org/1999/xhtml" lang="en">
        	<head>
        		<title>SVG 1.1 Test Suite - GPAC Implementation Status</title>
				<meta http-equiv="Content-Type" content="text/html; charset=utf-8"/>
        		<link rel="stylesheet" type="text/css" href="http://www.w3.org/StyleSheets/TR/base.css"/>
        		<style type="text/css" xml:space="preserve">
                    tr { padding: 0.2em; background: #ddd }
                    td {padding: 0.2em}
                    tr p {margin-top: 0.2em; margin-bottom: 0.2em}
                    .OK { background: #cfc; } 
                    .PARTIAL { background: #eee } 
                    .FAIL { background: #fdc; border: 2px solid #f33 } 
                </style>
        	</head>
            <body>
    
                <h1>SVG 1.1 Test Suite Implementation results (<xsl:value-of select="@date"/>)</h1>
                <h2><xsl:value-of select="@company"/> - <xsl:value-of select="@product"/> - <xsl:value-of select="@version"/></h2>
				<h2>Global results</h2>
				<table>
					<tbody>
						<tr>
							<td>Total number of tests</td>
							<td align="right"><xsl:value-of select="count(//test)"/></td>
						</tr>
						<tr>
							<td class="OK">Number of tests with status = OK</td>
							<td class="OK" align="right"><xsl:value-of select="count(//test[@status='OK'])"/></td>
						</tr>
						<tr>
							<td class="PARTIAL">Number of tests with status = PARTIAL</td>
							<td class="PARTIAL" align="right"><xsl:value-of select="count(//test[@status='PARTIAL'])"/></td>
						</tr>
						<tr>
							<td class="FAIL">Number of tests with status = FAIL</td>
							<td class="FAIL" align="right"><xsl:value-of select="count(//test[@status='FAIL'])"/></td>
						</tr>
					</tbody>
				</table>
				<h2>Detailled Results</h2>
                <table>
    				<thead>
    					<tr>
    						<th>Test File</th>
    						<th>Revision</th>
    						<th>Comment in case of failure or partial result.</th>
    					</tr>
    				</thead>
                    <tbody>
                    <xsl:apply-templates select="./*"/>
                    </tbody>
                </table>
            </body>
    	</html>
    </xsl:template>


<xsl:template match="test">
	<tr>	  
		<td class="{@status}" align="center"><xsl:value-of select="@name"/></td>
		<td class="{@status}" align="center"><xsl:value-of select="@revision"/></td>
		<td class="{@status}"><xsl:value-of select="@comment"/></td>
	</tr>
</xsl:template>

</xsl:stylesheet>

