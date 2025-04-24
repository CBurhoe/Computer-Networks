import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

def main():
    df = pd.read_csv(r'./netflow.csv')
    print(df)
    df_all_flows = df.__deepcopy__()

    print(df_all_flows)

    sns.ecdfplot(data=df_all_flows, x='Bytes', log_scale=True)
    plt.show()

    df_ip_flows = df_all_flows[df_all_flows['Protocol'] == 'IP']
    sns.ecdfplot(data=df_ip_flows, x='Bytes', log_scale=True)
    plt.show()

    df_udp_flows = df_all_flows[df_all_flows['Protocol'] == 'UDP']
    sns.ecdfplot(data=df_udp_flows, x='Bytes', log_scale=True)
    plt.show()

    # for index, row in df_all_flows_cdf.iterrows():
    #     df_tmp = pd.DataFrame(dict(Counter(df_all_flows_cdf['bytes'])), index = [0]).T




if __name__ == '__main__':
    main()