UPDATE Buggs 
SET [SummaryV2] = '' 
WHERE [SummaryV2] = 'KERNELBASE.dll'

UPDATE Crashes 
SET [Summary] = '' 
WHERE [Summary] = 'KERNELBASE.dll'


--OUTPUT $action, Inserted.*, Deleted.*