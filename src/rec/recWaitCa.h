typedef struct recWaitCa{
	char	*channame;
        char    inputIndex;
	void	(*callback)(struct recWaitCa *pcamonitor, char inputIndex, double monData);
	void	*userPvt;
	void	*recWaitCaPvt;
} RECWAITCA;

long recWaitCaAdd(RECWAITCA *pcamonitor);
long recWaitCaDelete(RECWAITCA *pcamonitor);
