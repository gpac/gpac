@ECHO OFF

SET XALAN="C:\Program Files\Java\xalan-j_2_7_0\xalan.jar"

@echo Creating HTML index
java -jar %XALAN% -IN index.xml -XSL auxiliary_files\index2html.xslt -OUT index.html 
@echo done.

@echo Creating Batch file for creation of HTML files
java -jar %XALAN% -IN index.xml -XSL auxiliary_files\index2batch.xslt -OUT create_htmls.bat -PARAM xalan_path %XALAN% -PARAM xslt_path auxiliary_files
@echo done.

call create_htmls.bat

