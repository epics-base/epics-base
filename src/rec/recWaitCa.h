typedef struct recWaitCa{
	char	*channame;
	void	(*callback)(struct recWaitCa *pcamonitor);
	void	*userPvt;
	void	*recWaitCaPvt;
} RECWAITCA;

long recWaitCaAdd(RECWAITCA *pcamonitor);
long recWaitCaDelete(RECWAITCA *pcamonitor);
