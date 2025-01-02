import math
import numpy
import pandas
import scipy.stats
import torch.nn
import torch.optim

def read_params_pandas( fname : str ) -> pandas.DataFrame :
    return pandas.DataFrame( numpy.loadtxt( fname ), columns = "t x c1 c2 b a".split() ) 

def regression_vars( df: pandas.DataFrame ) -> (pandas.DataFrame, pandas.DataFrame) : 
    n = 30*60
    x = pandas.DataFrame( dict(
        x = df.a-df.b,
        y = df.x.diff(n).shift(-n) ) ).dropna()
    train = x.iloc[:x.shape[0]//2] 
    test  = x.iloc[x.shape[0]//2:] 
    return train, test

def multiple_regr_vars( df: pandas.DataFrame ) -> (pandas.DataFrame, pandas.DataFrame ) : 
    data = dict( 
                x = df.a-df.b, # age difference on best offer minus best bid
            )
    for n in range(1, 60*60 , 30 ) : 
        data[f'y{n:d}'] = df.x.diff(n).shift(-n) # This is weigted mid difference looking forward n seconds
    x = pandas.DataFrame( data ).dropna()
    train = x.iloc[:x.shape[0]//2] 
    test  = x.iloc[x.shape[0]//2:] 
    return train, test
        

def regression(train: pandas.DataFrame, test: pandas.DataFrame) :
    r = scipy.stats.linregress( train.x, train.y )
    train_pred = r.intercept + r.slope * train.x
    test_pred  = r.intercept + r.slope * test.x
    r_train = numpy.sqrt( 1 - numpy.sum( numpy.square(train_pred - train.y) ) / numpy.sum( numpy.square( train.y ) ) )
    r_test = numpy.sqrt( 1 - numpy.sum( numpy.square(test_pred - test.y) ) / numpy.sum( numpy.square( test.y ) ) )
    return r_train, r_test, r
    
def calc_quantiles( x : pandas.Series ):
    dx = numpy.arange(0.1, 1, 0.1 )
    return list(zip(dx.tolist(), x.quantile(dx)))

def quantile_correlations( train : pandas.DataFrame, test: pandas.DataFrame ) : 
    columns = list( filter( lambda x : x[0]=='y' , train.columns ) )
    quantiles = calc_quantiles( train.x ) 
    corrs = dict()
    corrs['full'] = [ train.x.corr( train[col] ) for col in columns ]
    corrs_test = dict()
    corrs_test['full'] = [ test.x.corr( test[col] ) for col in columns ]
    for i, (q, tsh) in enumerate(quantiles) : 
        if i == 0 : 
            cond = train.x < tsh
            test_cond = test.x < tsh
            desc = f'x < {q:3.1f}'
        elif i == len(quantiles)-1 : 
            cond = train.x >= tsh 
            test_cond = test.x >= tsh 
            desc = f'x >= {q:3.1f}'
        else : 
            q_next, tsh_next = quantiles[i+1]
            cond = (train.x >= tsh) & (train.x < tsh_next )
            test_cond = (test.x >= tsh) & (test.x < tsh_next )
            desc = f'{q:3.1f} > x >= {q_next:3.1f}'
        df = train[cond]
        df_test = test[test_cond]
        corrs[desc] = [ df.x.corr( df[col] ) for col in columns ]
        corrs_test[desc] = [ df_test.x.corr( df_test[col] ) for col in columns ]
    return (
            pandas.DataFrame(corrs, index = [int(col[1:]) for col in columns] ) ,
            pandas.DataFrame(corrs_test, index = [int(col[1:]) for col in columns] )
            )

def regression_with_variance( train_x, train_y, test_x, test_y ) : 
    r = scipy.stats.linregress( train_x, train_y)
    err_train = train_y - ( train_x*r.slope + r.intercept )
    err_test = test_y - ( test_x*r.slope + r.intercept )
    return (
            math.sqrt( (err_train*err_train).mean() ), 
            math.sqrt( (err_test*err_test).mean() )
            )

def quantile_regressions( train : pandas.DataFrame, test: pandas.DataFrame ) : 
    quantiles = calc_quantiles( train.x ) 
    print(quantiles)
    columns = list( filter( lambda x : x[0]=='y' , train.columns ) )
    print(columns)
    index = [int(col[1:]) for col in columns]
    y_square_mean_sqrt_train = (train[columns]*train[columns]).mean().pow(1/2).rename( lambda y : int(y[1:]) )
    print(y_square_mean_sqrt_train)
    y_square_mean_sqrt_test  = (test[columns]*test[columns]).mean().pow(1/2).rename( lambda y : int(y[1:]) )
    print(y_square_mean_sqrt_test)
    err_train_full = dict()
    err_test_full = dict()
    err_train, err_test = list(zip(*[
        regression_with_variance(
            train.x, train[col], 
            test.x, test[col])
        for col in columns
        ]))
    err_train_full['full'] = pandas.Series( err_train , index=index )/y_square_mean_sqrt_train 
    err_test_full ['full'] = pandas.Series( err_test  , index=index )/y_square_mean_sqrt_test 
    if True : 
        for i, (q, tsh) in enumerate(quantiles) : 
            if i == 0 : 
                cond = train.x < tsh
                test_cond = test.x < tsh
                desc = f'x<{q:3.1f}'
            elif i == len(quantiles)-1 : 
                cond = train.x >= tsh 
                test_cond = test.x >= tsh 
                desc = f'x>={q:3.1f}'
            else : 
                q_next, tsh_next = quantiles[i+1]
                cond = (train.x >= tsh) & (train.x < tsh_next )
                test_cond = (test.x >= tsh) & (test.x < tsh_next )
                desc = f'{q:3.1f}>x >={q_next:3.1f}'
            df_train = train[cond]
            df_test  = test [test_cond]
            err_train, err_test = list(zip(*[
                regression_with_variance(
                    df_train.x, df_train[col], 
                    df_test.x, df_test[col])
                for col in columns
                ]))
            err_train_full[desc] = pandas.Series( err_train , index=index )/y_square_mean_sqrt_train 
            err_test_full [desc] = pandas.Series( err_test  , index=index )/y_square_mean_sqrt_test 
            print(desc,err_train_full[desc])
    return (
            pandas.DataFrame(err_train_full), 
            pandas.DataFrame(err_test_full), 
            )









def rsquared(yp,y) : 
    return 1-torch.mean( torch.square( yp-y ) )/torch.mean( torch.square( y ) ) 



def simple_nn(train: pandas.DataFrame, test: pandas.DataFrame) :
    """ this is just regression"""
    class SimpleNN( torch.nn.Module ) : 
        def __init__(self) : 
            super().__init__() 
            self.linear = torch.nn.Linear( 1, 1 ) 
            self.activation = torch.nn.Tanh() 
        def forward(self,x) : 
            x1 = self.linear(x)
            #print(x.shape, x1.shape, self.linear.state_dict())
            #raise Exception()
            x2 = self.activation(x1)
            return x2
    x = torch.Tensor( train.x.values ).unsqueeze(1)
    y = torch.Tensor( train.y.values ).unsqueeze(1)
    x_test = torch.Tensor( test.x.values ).unsqueeze(1)
    y_test = torch.Tensor( test.y.values ).unsqueeze(1)
    model = SimpleNN()
    optimizer = torch.optim.Adam(model.parameters())
    criterion = torch.nn.MSELoss()
    model.train()
    prev_loss = 1e9
    diff = 1e9
    while diff > 1e-14 : 
        optimizer.zero_grad()
        pred = model( x )
        #loss = (pred - y).square().mean() # criterion( pred , y )
        loss = criterion( pred , y )
        loss.backward()
        optimizer.step()
        diff = abs( prev_loss - loss.item() )
        prev_loss = loss.item()
        intercept = model.linear.state_dict()['bias'].item()
        slope = model.linear.state_dict()['weight'].item()
        rs = rsquared(pred, y).item()
        pred_test = model( x_test )
        rs_test = rsquared(pred_test, y_test).item()
        print(f"loss: {loss.item():>7f}, rs:{rs:>7f}, r:{numpy.sqrt(abs(rs)):>7f}, rst:{rs_test:>7f}, rt:{numpy.sqrt(abs(rs_test)):>7f}, diff:{diff:1.3e} slope:{slope:1.3e}, int:{intercept:1.3e}")


def simple_nn2(train: pandas.DataFrame, test: pandas.DataFrame) :
    """ multilayer ann. surprisingly faster than simple single linear layer ann, i.e. regression."""
    class SimpleNN( torch.nn.Module ) : 
        def __init__(self) : 
            super().__init__() 
            self.linear1 = torch.nn.Linear( 1, 10 ) 
            self.activation1 = torch.nn.Tanh() 
            self.linear2 = torch.nn.Linear( 10, 10 ) 
            self.activation2 = torch.nn.Tanh() 
            self.linear3 = torch.nn.Linear( 10, 1 ) 
            self.activation3 = torch.nn.Tanh() 
        def forward(self,x) : 
            return self.activation3( self.linear3(
                self.activation2( self.linear2(
                    self.activation1( self.linear1( x ) ) ) ) ) )
    x = torch.Tensor( train.x.values ).unsqueeze(1)
    y = torch.Tensor( train.y.values ).unsqueeze(1)
    x_test = torch.Tensor( test.x.values ).unsqueeze(1)
    y_test = torch.Tensor( test.y.values ).unsqueeze(1)
    model = SimpleNN()
    optimizer = torch.optim.Adam(model.parameters())
    criterion = torch.nn.MSELoss()
    model.train()
    prev_loss = 1e9
    diff = 1e9
    while diff > 1e-14 : 
        optimizer.zero_grad()
        pred = model( x )
        loss = criterion( pred , y )
        loss.backward()
        optimizer.step()
        diff = abs( prev_loss - loss.item() )
        prev_loss = loss.item()
        rs = rsquared(pred, y).item()
        pred_test = model( x_test )
        rs_test = rsquared(pred_test, y_test).item()
        print(f"loss: {loss.item():>7f}, rs:{rs:>7f}, r:{numpy.sqrt(abs(rs)):>7f}, rst:{rs_test:>7f}, rt:{numpy.sqrt(abs(rs_test)):>7f}, diff:{diff:1.3e}")


def main(fname: str) : 
    df = read_params_pandas( fname )
    train, test = regression_vars( df )
    reg_train, reg_test, _ = regression( train, test ) 
    print(f"regression result: train {reg_train:1.4f}, test {reg_test:1.4f}"  )
    

