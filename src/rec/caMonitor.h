typedef struct caMonitor{
	char	*channame;
	void	(*callback)(struct caMonitor *pcamonitor);
	void	*userPvt;
	void	*caMonitorPvt;
} CAMONITOR;

long caMonitorAdd(CAMONITOR *pcamonitor);
long caMonitorDelete(CAMONITOR *pcamonitor);
