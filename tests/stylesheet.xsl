<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:template match="/">
  <html><body>
    <h3>Version: <xsl:value-of select="GPACTestSuite/@version"/> Platform: <xsl:value-of select="GPACTestSuite/@platform"/> Start: <xsl:value-of select="GPACTestSuite/@start_date"/> End: <xsl:value-of select="GPACTestSuite/@end_date"/></h3>
    <h3>Test: <xsl:value-of select="GPACTestSuite/TestSuiteResults/@NumTests"/>   Passed: <xsl:value-of select="GPACTestSuite/TestSuiteResults/@TestsPassed"/>   Failed: <xsl:value-of select="GPACTestSuite/TestSuiteResults/@TestsFailed"/> Leaked: <xsl:value-of select="GPACTestSuite/TestSuiteResults/@TestsLeaked"/></h3>

    <table border="1">
      <tr bgcolor="#80c0c0">
        <th>Name</th>
        <th>Execution Status</th>
        <th>Execution Logs</th>
      </tr>
      <xsl:for-each select="GPACTestSuite/test">
      <tr>
        <xsl:choose>
          <xsl:when test="@result='OK'">
			<td><font color="green"><xsl:value-of select="@name"/></font></td>
            <td><a href="logs/{@name}-passed.xml"> <font color="green"><xsl:value-of select="@result"/></font></a></td>
          </xsl:when>
          <xsl:when test="@result='play:MemLeak '">
			<td><font color="orange"><xsl:value-of select="@name"/></font></td>
            <td><font color="orange"><xsl:value-of select="@result"/></font></td>
          </xsl:when>
          <xsl:otherwise>
			<td><font color="red"><xsl:value-of select="@name"/></font></td>
            <td><font color="red"><xsl:value-of select="@result"/></font></td>
          </xsl:otherwise>
        </xsl:choose>
        <td><a href="logs/{@name}-logs.txt"> View detail</a></td>
      </tr>
      </xsl:for-each>
    </table>
  </body></html>
</xsl:template>

</xsl:stylesheet>
