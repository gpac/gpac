@ECHO OFF

SET XALAN="C:\Users\Cyril\CVSEnst\sserbe\lib\xalan.jar"

@echo Creating Batch file for creation of HTML files
java -jar %XALAN% -IN index.xml -XSL auxiliary_files\index2batch.xslt -OUT create_htmls.bat -PARAM xalan_path %XALAN% -PARAM xslt_path auxiliary_files
@echo done.

call create_htmls.bat

@echo Creating HTML index
java -jar %XALAN% -IN index.xml -XSL auxiliary_files\index2html.xslt -OUT index.html 
@echo done.

